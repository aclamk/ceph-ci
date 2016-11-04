// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_LIBRBD_EXCLUSIVE_LOCK_H
#define CEPH_LIBRBD_EXCLUSIVE_LOCK_H

#include "librbd/ManagedLock.h"
#include "librbd/ImageCtx.h"

namespace librbd {

template <typename ImageCtxT = ImageCtx>
class ExclusiveLock : public ManagedLock<ImageCtxT> {
public:
  static ExclusiveLock *create(ImageCtxT &image_ctx) {
    return new ExclusiveLock<ImageCtxT>(image_ctx);
  }

  ExclusiveLock(ImageCtxT &image_ctx);

  bool accept_requests(int *ret_val) const;

  void block_requests(int r);
  void unblock_requests();

  void init(uint64_t features, Context *on_init);
  void shut_down(Context *on_shutdown);

  void handle_peer_notification();

protected:
  virtual void shutdown_handler(int r, Context *on_finish);
  virtual void pre_acquire_lock_handler(Context *on_finish);
  virtual void post_acquire_lock_handler(int r, Context *on_finish);
  virtual void pre_release_lock_handler(bool shutting_down,
                                        Context *on_finish);
  virtual void post_release_lock_handler(bool shutting_down, int r,
                                         Context *on_finish);

private:

  /**
   * @verbatim
   *
   * <start>                              * * > WAITING_FOR_REGISTER --------\
   *    |                                 * (watch not registered)           |
   *    |                                 *                                  |
   *    |                                 * * > WAITING_FOR_PEER ------------\
   *    |                                 * (request_lock busy)              |
   *    |                                 *                                  |
   *    |                                 * * * * * * * * * * * * * *        |
   *    |                                                           *        |
   *    v            (init)            (try_lock/request_lock)      *        |
   * UNINITIALIZED  -------> UNLOCKED ------------------------> ACQUIRING <--/
   *                            ^                                   |
   *                            |                                   v
   *                         RELEASING                        POST_ACQUIRING
   *                            |                                   |
   *                            |                                   |
   *                            |          (release_lock)           v
   *                      PRE_RELEASING <------------------------ LOCKED
   *
   * <LOCKED state>
   *    |
   *    v
   * REACQUIRING -------------------------------------> <finish>
   *    .                                                 ^
   *    .                                                 |
   *    . . . > <RELEASE action> ---> <ACQUIRE action> ---/
   *
   * <UNLOCKED/LOCKED states>
   *    |
   *    |
   *    v
   * PRE_SHUTTING_DOWN ---> SHUTTING_DOWN ---> SHUTDOWN ---> <finish>
   *
   * @endverbatim
   */

  struct C_InitComplete : public Context {
    ExclusiveLock *exclusive_lock;
    Context *on_init;
    C_InitComplete(ExclusiveLock *exclusive_lock, Context *on_init)
      : exclusive_lock(exclusive_lock), on_init(on_init) {
    }
    virtual void finish(int r) override {
      if (r == 0) {
        exclusive_lock->handle_init_complete();
      }
      on_init->complete(r);
    }
  };

  ImageCtxT& m_image_ctx;
  Context *m_pre_post_callback;
  bool m_shutting_down;

  uint32_t m_request_blocked_count = 0;
  int m_request_blocked_ret_val = 0;

  void handle_init_complete();
  void handle_post_acquiring_lock(int r);
  void handle_post_acquired_lock(int r);
  void handle_pre_releasing_lock(int r);
};

} // namespace librbd

#endif // CEPH_LIBRBD_EXCLUSIVE_LOCK_H
