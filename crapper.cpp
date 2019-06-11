/**
 * @brief REST Aggregation Protocol test server
 * @author Johan Lindh <johan@linkdata.se>
 * @note Copyright (c)2015-2017 Johan Lindh
 */

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

#include "rap.hpp"
#include "rap_conn.hpp"
#include "rap_muxer.hpp"
#include "rap_reader.hpp"
#include "rap_request.hpp"
#include "rap_response.hpp"
#include "rap_stats.hpp"

/* crap.h must be included after rap.hpp */
#include "crap.h"

#define PRINT_STREAM stderr
#define PRINT_NETDATA 1

#if PRINT_NETDATA
static void print_netdata(char rw, const char* src_ptr, const char* src_end) {
    while (src_ptr < src_end) {
        const char* p = src_ptr;
        int n = 0;
        fprintf(PRINT_STREAM, "%c: ", rw);
        while (p < src_end && n++ < 16)
            fprintf(PRINT_STREAM, "%02x ", (*p++) & 0xFF);
        while (n++ < 16)
            fprintf(PRINT_STREAM, "   ");
        fputc(' ', PRINT_STREAM);
        p = src_ptr;
        n = 0;
        while (p < src_end && n++ < 16) {
            int ch = (*p++) & 0xFF;
            if (!isgraph(ch)) ch = '.';
            fputc(ch, PRINT_STREAM);
        }
        fputc('\n', PRINT_STREAM);
        src_ptr += 16;
    }
    fflush(PRINT_STREAM);
}
#endif


using boost::asio::ip::tcp;

class conn : public std::streambuf {
public:
    explicit conn()
        : conn_(nullptr)
        , stats_(nullptr)
        , contentlength_(-1)
        , contentread_(0)
        , id_(rap_muxer_conn_id)
    {
    }

    void init(rap_conn* conn, rap::stats* stats = nullptr)
    {
        conn_ = conn;
        stats_ = stats;
        if (conn_) {
            id_ = rap_conn_get_id(conn_);
            rap_conn_set_callback(conn, s_conn_cb, this);
            finalframe_.set_id(id_);
            finalframe_.set_final();
        }
        start_write();
    }

    rap_conn_id id() const { return id_; }

    const rap_header& header() const
    {
        return *reinterpret_cast<const rap_header*>(buf_.data());
    }

    rap_header& header() { return *reinterpret_cast<rap_header*>(buf_.data()); }

    static int s_conn_cb(void* conn_cb_param, rap_conn* conn,
        const rap_frame* f, int len)
    {
        return static_cast<class conn*>(conn_cb_param)->conn_cb(conn, f, len);
    }

    int conn_cb(rap_conn* /*conn*/, const rap_frame* f, int len)
    {
        // TODO: thread safety?
        assert(f != nullptr);
        assert(len >= rap_frame_header_size);
        assert(len == rap_frame_header_size + static_cast<int>(f->header().payload_size()));

        const rap_header& hdr = f->header();
        rap::reader r(f);
        if (hdr.has_head()) {
            if (stats_)
                stats_->head_count++;
            process_head(r);
        }
        if (hdr.is_final() || (contentlength_ >= 0 && contentread_ >= contentlength_))
            write_frame(finalframe_);
        else if (hdr.has_body())
            process_body(r);
        pubsync();
        return 0;
    }

    rap::error process_head(rap::reader& r)
    {
        if (r.read_tag() != rap::record::tag_http_request)
            return rap::rap_err_unknown_frame_type;
        rap::request req(r);
        req_echo_.clear();
        req.render(req_echo_);
        header().set_head();
        contentread_ = 0;
        contentlength_ = req.content_length();
        rap::writer(*this) << rap::response(200, contentlength_ + static_cast<int64_t>(req_echo_.size()));
        header().set_body();
        sputn(req_echo_.data(), static_cast<std::streamsize>(req_echo_.size()));
        return r.error();
    }

    rap::error process_body(rap::reader& r)
    {
        assert(r.size() > 0);
        header().set_body();
        sputc('\n');
        sputn(r.data(), static_cast<std::streamsize>(r.size()));
        contentread_ += r.size();
        r.consume();
        return r.error();
    }

    /*
    rap::error process_final(rap::reader& r)
    {
        assert(r.size() == 0);
        header().set_final();
        pubsync();
        return r.error();
    }
    */

protected:
    conn::int_type overflow(int_type ch)
    {
        if (buf_.size() < rap_frame_max_size) {
            size_t new_size = buf_.size() * 2;
            if (new_size > rap_frame_max_size)
                new_size = rap_frame_max_size;
            buf_.resize(new_size);
            setp(buf_.data() + rap_frame_header_size, buf_.data() + buf_.size());
        }

        bool was_head = header().has_head();
        bool was_body = header().has_body();

        if (sync() != 0) {
            return traits_type::eof();
        }

        if (was_body)
            header().set_body();
        else if (was_head)
            header().set_head();

        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }

        return ch;
    }

    int write_frame(const rap_frame* f) {
        int r = rap_conn_write_frame(conn_, f);
        if (r) {
            fprintf(PRINT_STREAM, "crapper::conn::write_frame(): error %d", r);
            assert(r == 0);
        }
        return r;
    }
    int write_frame(const rap_header& h) { return write_frame(reinterpret_cast<const rap_frame*>(&h)); }
    int write_frame(const std::vector<char>& v) { return write_frame(reinterpret_cast<const rap_frame*>(v.data())); }

    int sync()
    {
        header().set_size_value(static_cast<size_t>(pptr() - (buf_.data() + rap_frame_header_size)));
        if (write_frame(buf_))
            return -1;
        start_write();
        return 0;
    }

private:
    rap_conn* conn_;
    rap::stats* stats_;
    std::vector<char> buf_;
    rap::string_t req_echo_;
    int64_t contentlength_;
    int64_t contentread_;
    rap_conn_id id_;
    rap_header finalframe_;

    void start_write()
    {
        if (buf_.size() < 256)
            buf_.resize(256);
        buf_[0] = '\0';
        buf_[1] = '\0';
        buf_[2] = static_cast<char>(id_ >> 8);
        buf_[3] = static_cast<char>(id_);
        setp(buf_.data() + rap_frame_header_size, buf_.data() + buf_.size());
    }
};

class session : public std::enable_shared_from_this<session> {
public:
    session(tcp::socket socket, rap::stats& stats)
        : socket_(std::move(socket))
        , conns_(rap_max_conn_id + 1)
        , muxer_(nullptr)
        , stats_(stats)
    {
    }

    ~session()
    {
        if (muxer_) {
            rap_muxer_destroy(muxer_);
            muxer_ = nullptr;
        }
    }

    void start()
    {
        if (!muxer_) {
            muxer_ = rap_muxer_create(this, s_write_cb, s_conn_init_cb);
        }
        read_stream();
    }

private:
    static int s_write_cb(void* self, const char* src_ptr, int src_len)
    {
        return static_cast<session*>(self)->write_cb(src_ptr, src_len);
    }

    static void s_conn_init_cb(void* self, rap_conn_id id, rap_conn* conn)
    {
        static_cast<session*>(self)->conn_init(id, conn);
    }

    // may be called from a foreign thread via the callback
    int write_cb(const char* src_ptr, int src_len)
    {
        std::lock_guard<std::mutex> g(write_mtx_);
        buf_towrite_.insert(buf_towrite_.end(), src_ptr, src_ptr + src_len);
        write_some();
        return 0;
    }

    void conn_init(rap_conn_id id, rap_conn* conn)
    {
        conns_[id].init(conn, &stats_);
    }

    // writes any buffered data to the stream using muxer_t::write_stream()
    // may be called from a foreign thread via the callback
    void write_some()
    {
        if (!buf_writing_.empty() || buf_towrite_.empty())
            return;
        buf_writing_.swap(buf_towrite_);
        write_stream(buf_writing_.data(), buf_writing_.size());
        return;
    }

    void write_stream(const char* src_ptr, size_t src_len)
    {
        auto self(shared_from_this());

#if PRINT_NETDATA
        print_netdata('W', src_ptr, src_ptr + src_len);
#endif

        boost::asio::async_write(
            socket_, boost::asio::buffer(src_ptr, src_len),
            [this, self](boost::system::error_code ec, std::size_t length) {
                std::lock_guard<std::mutex> g(write_mtx_);
                if (ec) {
                    fprintf(PRINT_STREAM, "crapper::muxer::write_stream(%s, %lu)\n",
                        ec.message().c_str(), static_cast<unsigned long>(length));
                    fflush(PRINT_STREAM);
                    return;
                } else {
                    assert(length == buf_writing_.size());
                    stats_.add_bytes_written(length);
                }
                buf_writing_.clear();
                write_some();
            });
        return;
    }

    void read_stream()
    {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (ec) {
                    if (ec != boost::asio::error::eof) {
                        fprintf(PRINT_STREAM, "crapper::muxer::read_stream(%s, %lu) [%d]\n",
                            ec.message().c_str(), static_cast<unsigned long>(length),
                            ec.value());
                        fflush(PRINT_STREAM);
                    }
                    return;
                }
                stats_.add_bytes_read(length);
#if PRINT_NETDATA
                print_netdata('R', data_, data_ + length);
#endif
                int rap_ec = rap_muxer_recv(muxer_, data_, static_cast<int>(length));
                if (rap_ec < 0) {
                    fprintf(PRINT_STREAM, "crapper::muxer::read_stream(): rap error %d\n",
                        rap_ec);
                    fflush(PRINT_STREAM);
                } else {
                    assert(rap_ec == static_cast<int>(length));
                }

                read_stream();
            });
    }

    enum {
        max_length = 4096
    };
    tcp::socket socket_;
    char data_[max_length];
    std::mutex write_mtx_; // guards the buffers below
    std::vector<char> buf_towrite_;
    std::vector<char> buf_writing_;
    std::vector<conn> conns_;
    rap_muxer* muxer_;
    rap::stats& stats_;
};

class server {
public:
    server(unsigned short port)
        : last_head_count_(0)
        , last_read_iops_(0)
        , last_read_bytes_(0)
        , last_write_iops_(0)
        , last_write_bytes_(0)
        , last_stat_mbps_in_(0)
        , last_stat_mbps_out_(0)
        , last_stat_rps_(0)
        , timer_(io_service_)
        , acceptor_(io_service_, tcp::endpoint(tcp::v4(), port))
        , socket_(io_service_)
        , thread_pool_size_(1)
    {
        do_timer();
        do_accept();
    }

    void run()
    {
        if (thread_pool_size_ > 1) {
            // Create a pool of threads to run all of the io_services.
            std::vector<boost::shared_ptr<boost::thread>> threads;
            for (std::size_t i = 0; i < thread_pool_size_; ++i) {
                boost::shared_ptr<boost::thread> thread(new boost::thread(
                    boost::bind(&boost::asio::io_service::run, &io_service_)));
                threads.push_back(thread);
            }

            // Wait for all threads in the pool to exit.
            for (std::size_t i = 0; i < threads.size(); ++i)
                threads[i]->join();
        } else {
            io_service_.run();
        }
    }

protected:
    uint64_t last_head_count_;
    uint64_t last_read_iops_;
    uint64_t last_read_bytes_;
    uint64_t last_write_iops_;
    uint64_t last_write_bytes_;
    uint64_t last_stat_mbps_in_;
    uint64_t last_stat_mbps_out_;
    uint64_t last_stat_rps_;

private:
    void do_timer()
    {
        timer_.expires_from_now(boost::posix_time::seconds(1));
        timer_.async_wait(
            [this](const boost::system::error_code ec) { handle_timeout(ec); });
    }

    void do_accept()
    {
        acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
            if (!ec) {
                boost::asio::ip::tcp::no_delay no_delay_option;
                boost::asio::socket_base::receive_buffer_size
                    receive_buffer_size_option;
                boost::asio::socket_base::send_buffer_size send_buffer_size_option;
                socket_.get_option(no_delay_option);
                socket_.get_option(receive_buffer_size_option);
                socket_.get_option(send_buffer_size_option);
                fprintf(PRINT_STREAM,
                    "connection established (no_delay %d, read_stream %d, send %d)\n",
                    no_delay_option.value(),
                    receive_buffer_size_option.value(),
                    send_buffer_size_option.value());
                std::make_shared<session>(std::move(socket_), stats_)->start();
            }
            do_accept();
        });
    }

    void handle_timeout(const boost::system::error_code& e)
    {
        if (e != boost::asio::error::operation_aborted) {
            unsigned long long n;
            unsigned long long stat_rps_;
            unsigned long long stat_iops_in_;
            unsigned long long stat_mbps_in_;
            unsigned long long stat_iops_out_;
            unsigned long long stat_mbps_out_;
            unsigned long long stat_bytes_per_write_ = 0;

            n = stats_.head_count;
            stat_rps_ = n - last_head_count_;
            last_head_count_ = n;

            n = stats_.read_iops;
            stat_iops_in_ = n - last_read_iops_;
            last_read_iops_ = n;

            n = stats_.read_bytes;
            stat_mbps_in_ = ((n - last_read_bytes_) * 8) / 1024 / 1024;
            last_read_bytes_ = n;

            n = stats_.write_iops;
            stat_iops_out_ = n - last_write_iops_;
            last_write_iops_ = n;

            n = stats_.write_bytes;
            stat_mbps_out_ = ((n - last_write_bytes_) * 8) / 1024 / 1024;
            if (stat_iops_out_ > 0)
                stat_bytes_per_write_ = (n - last_write_bytes_) / stat_iops_out_;
            last_write_bytes_ = n;

            if (stat_mbps_in_ != last_stat_mbps_in_ || stat_mbps_out_ != last_stat_mbps_out_ || stat_rps_ != last_stat_rps_) {
                last_stat_mbps_in_ = stat_mbps_in_;
                last_stat_mbps_out_ = stat_mbps_out_;
                last_stat_rps_ = stat_rps_;
                fprintf(
                    PRINT_STREAM,
                    "%llu Rps - IN: %llu Mbps, %llu iops - OUT: %llu Mbps, %llu iops, %llu bpio\n",
                    stat_rps_, stat_mbps_in_, stat_iops_in_, stat_mbps_out_,
                    stat_iops_out_, stat_bytes_per_write_);
            }
        }
        do_timer();
    }

    boost::asio::io_service io_service_;
    boost::asio::deadline_timer timer_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    rap::stats stats_;
    size_t thread_pool_size_;
};

int main(int argc, char* argv[])
{
    const char* port = "10111";
    try {
        if (argc == 2) {
            port = argv[1];
        }
        server s(static_cast<unsigned short>(std::atoi(port)));
        s.run();
    } catch (std::exception& e) {
        fprintf(PRINT_STREAM, "Exception: %s\n", e.what());
    }

    return 0;
}
