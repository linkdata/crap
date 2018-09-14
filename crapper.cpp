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
#include "rap_exchange.hpp"
#include "rap_reader.hpp"
#include "rap_request.hpp"
#include "rap_response.hpp"
#include "rap_stats.hpp"

/* crap.h must be included after rap.hpp */
#include "crap.h"

using boost::asio::ip::tcp;

class exchange : public std::streambuf {
public:
    explicit exchange()
        : exch_(nullptr)
        , stats_(nullptr)
        , id_(rap_conn_exchange_id)
        , contentlength_(-1)
        , contentread_(0)
    {
    }

    void init(rap_exchange* exch, rap::stats* stats = nullptr)
    {
        exch_ = exch;
        stats_ = stats;
        if (exch_) {
            id_ = rap_exch_get_id(exch_);
            rap_exch_set_callback(exch, s_exchange_cb, this);
        }
        start_write();
    }

    rap_exch_id id() const { return id_; }

    const rap_header& header() const
    {
        return *reinterpret_cast<const rap_header*>(buf_.data());
    }

    rap_header& header() { return *reinterpret_cast<rap_header*>(buf_.data()); }

    static int s_exchange_cb(void* exchange_cb_param, rap_exchange* exch,
        const rap_frame* f, int len)
    {
        return static_cast<exchange*>(exchange_cb_param)->exchange_cb(exch, f, len);
    }

    int exchange_cb(rap_exchange* /*exch*/, const rap_frame* f, int len)
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
        if (hdr.has_body())
            process_body(r);
        pubsync();
        if (contentread_ >= contentlength_) {
            header().set_final();
            pubsync();
        }
        return 0;
    }

    rap::error process_head(rap::reader& r)
    {
        if (r.read_tag() != rap::record::tag_http_request)
            return rap::rap_err_unknown_frame_type;
        rap::request req(r);
        req_echo_.clear();
        req.render(req_echo_);
        req_echo_ += '\n';
        header().set_head();
        contentread_ = 0;
        contentlength_ = req.content_length();
        rap::writer(*this) << rap::response(200, req_echo_.size() + contentlength_);
        header().set_body();
        sputn(req_echo_.data(), static_cast<std::streamsize>(req_echo_.size()));
        return r.error();
    }

    rap::error process_body(rap::reader& r)
    {
        assert(r.size() > 0);
        header().set_body();
        sputn(r.data(), static_cast<std::streamsize>(r.size()));
        contentread_ += r.size();
        r.consume();
        return r.error();
    }

    rap::error process_final(rap::reader& r)
    {
        assert(r.size() == 0);
        header().set_final();
        pubsync();
        return r.error();
    }

protected:
    exchange::int_type overflow(int_type ch)
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
        bool was_final = header().is_final();

        // if (was_final && ch != traits_type::eof())
        header().clr_final();

        if (sync() != 0) {
            if (was_final)
                header().set_final();
            return traits_type::eof();
        }

        if (was_body)
            header().set_body();
        if (was_head)
            header().set_head();

        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        } else {
            if (was_final)
                header().set_final();
        }

        return ch;
    }

    int write_frame(const rap_frame* f) { return rap_exch_write_frame(exch_, f); }

    int sync()
    {
        header().set_size_value(static_cast<size_t>(pptr() - (buf_.data() + rap_frame_header_size)));
        if (write_frame(reinterpret_cast<rap_frame*>(buf_.data()))) {
            assert(nullptr == "rap::exchange::sync(): write_frame() failed");
            return -1;
        }
        start_write();
        return 0;
    }

private:
    rap_exchange* exch_;
    rap::stats* stats_;
    std::vector<char> buf_;
    rap::string_t req_echo_;
    int64_t contentlength_;
    int64_t contentread_;
    rap_exch_id id_;

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
        , exchanges_(rap_max_exchange_id + 1)
        , conn_(nullptr)
        , stats_(stats)
    {
    }

    ~session()
    {
        if (conn_) {
            rap_conn_destroy(conn_);
            conn_ = nullptr;
        }
    }

    void start()
    {
        if (!conn_) {
            conn_ = rap_conn_create(this, s_write_cb, s_exch_init_cb);
        }
        read_stream();
    }

private:
    static int s_write_cb(void* self, const char* src_ptr, int src_len)
    {
        return static_cast<session*>(self)->write_cb(src_ptr, src_len);
    }

    static void s_exch_init_cb(void* self, rap_exch_id id, rap_exchange* exch)
    {
        static_cast<session*>(self)->exch_init(id, exch);
    }

    // may be called from a foreign thread via the callback
    int write_cb(const char* src_ptr, int src_len)
    {
        // TODO: thread safety?
        buf_towrite_.insert(buf_towrite_.end(), src_ptr, src_ptr + src_len);
        write_some();
        return 0;
    }

    void exch_init(rap_exch_id id, rap_exchange* exch)
    {
        exchanges_[id].init(exch, &stats_);
    }

    // writes any buffered data to the stream using conn_t::write_stream()
    // may be called from a foreign thread via the callback
    void write_some()
    {
        // TODO: thread safety?
        if (!buf_writing_.empty() || buf_towrite_.empty())
            return;
        buf_writing_.swap(buf_towrite_);
        write_stream(buf_writing_.data(), buf_writing_.size());
        return;
    }

    void write_stream(const char* src_ptr, size_t src_len)
    {
        auto self(shared_from_this());

#if 0
    const char* src_end = src_ptr + src_len;
    const char* p = src_ptr;
    fprintf(stderr, "W: ");
    while (p < src_end) {
      fprintf(stderr, "%02x ", (*p++) & 0xFF);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif

        boost::asio::async_write(
            socket_, boost::asio::buffer(src_ptr, src_len),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (ec) {
                    fprintf(stderr, "rapper::conn::write_stream(%s, %lu)\n",
                        ec.message().c_str(), static_cast<unsigned long>(length));
                    fflush(stderr);
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
                        fprintf(stderr, "rapper::conn::read_stream(%s, %lu) [%d]\n",
                            ec.message().c_str(), static_cast<unsigned long>(length),
                            ec.value());
                        fflush(stderr);
                    }
                    return;
                }
                stats_.add_bytes_read(length);
#if 0
          const char* src_end = data_ + length;
          const char* p = data_;
          fprintf(stderr, "R: ");
          while (p < src_end) {
            fprintf(stderr, "%02x ", (*p++) & 0xFF);
          }
          fprintf(stderr, "\n");
          fflush(stderr);
#endif

                int rap_ec = rap_conn_recv(conn_, data_, static_cast<int>(length));
                if (rap_ec < 0) {
                    fprintf(stderr, "rapper::conn::read_stream(): rap error %d\n",
                        rap_ec);
                    fflush(stderr);
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
    std::vector<char> buf_towrite_;
    std::vector<char> buf_writing_;
    std::vector<exchange> exchanges_;
    rap_conn* conn_;
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
                /*
                fprintf(stderr,
                        "connection established (no_delay %d, read_stream %d, send
           %d)\n", no_delay_option.value(), receive_buffer_size_option.value(),
                        send_buffer_size_option.value());
        */
                std::make_shared<session>(std::move(socket_), stats_)->start();
            }
            do_accept();
        });
    }

    void handle_timeout(const boost::system::error_code& e)
    {
        if (e != boost::asio::error::operation_aborted) {
            uint64_t n;
            uint64_t stat_rps_;
            uint64_t stat_iops_in_;
            uint64_t stat_mbps_in_;
            uint64_t stat_iops_out_;
            uint64_t stat_mbps_out_;

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
            last_write_bytes_ = n;

            if (stat_mbps_in_ != last_stat_mbps_in_ || stat_mbps_out_ != last_stat_mbps_out_ || stat_rps_ != last_stat_rps_) {
                last_stat_mbps_in_ = stat_mbps_in_;
                last_stat_mbps_out_ = stat_mbps_out_;
                last_stat_rps_ = stat_rps_;
                fprintf(
                    stderr,
                    "%lu Rps - IN: %lu Mbps, %lu iops - OUT: %lu Mbps, %lu iops\n",
                    stat_rps_, stat_mbps_in_, stat_iops_in_, stat_mbps_out_,
                    stat_iops_out_);
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
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
