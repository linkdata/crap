#ifndef RAP_FRAME_H
#define RAP_FRAME_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rap_header.h"

enum
{
  rap_frame_max_size = 0x10000, /**< Maximum frame size on the wire. */
  rap_frame_max_payload_size =
      rap_frame_max_size -
      rap_frame_header_size, /**< Maximum allowed frame payload size. */
};

struct rap_frame
{
  static size_t needed_bytes(const char *src_ptr)
  {
    return reinterpret_cast<const rap_header *>(src_ptr)->size();
  }

  static rap_frame *factory(const char *src_ptr, size_t src_len)
  {
    assert(src_ptr != NULL);
    assert(rap_frame_header_size <= src_len);
    assert(needed_bytes(src_ptr) == src_len);
    return reinterpret_cast<const rap_frame *>(src_ptr)->copy();
  }

  rap_frame *copy() const
  {
    size_t src_len = size();
    if (rap_frame *f = static_cast<rap_frame *>(malloc(src_len)))
    {
      memcpy(f, this, src_len);
      assert(f->size() == src_len);
      return f;
    }
    return NULL;
  }

  const rap_header &header() const
  {
    return *reinterpret_cast<const rap_header *>(this);
  }
  rap_header &header() { return *reinterpret_cast<rap_header *>(this); }
  const char *data() const { return reinterpret_cast<const char *>(this); }
  char *data() { return reinterpret_cast<char *>(this); }
  size_t size() const { return header().size(); }

  bool has_payload() const { return header().has_payload(); }
  size_t payload_size() const { return header().payload_size(); }
  const char *payload() const { return data() + rap_frame_header_size; }
  char *payload() { return data() + rap_frame_header_size; }
};

struct framelink
{
  static void enqueue(framelink **pp_fl, const rap_frame *f)
  {
    if (f != NULL)
    {
      size_t framesize = f->size();
      if (framelink *p_fl = reinterpret_cast<framelink *>(
              malloc(sizeof(framelink) + framesize)))
      {
        p_fl->next = NULL;
        memcpy(p_fl + 1, f, framesize);
        while (*pp_fl)
          pp_fl = &((*pp_fl)->next);
        *pp_fl = p_fl;
      }
    }
  }

  static void dequeue(framelink **pp_fl)
  {
    while (framelink *p_fl = *pp_fl)
    {
      if (p_fl->next == NULL)
      {
        *pp_fl = NULL;
        free(p_fl);
        return;
      }
      pp_fl = &(p_fl->next);
    }
    return;
  }

  framelink *next;
};

#endif // RAP_FRAME_H
