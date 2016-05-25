// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_PROXY_H_
#define IPC_IPC_CHANNEL_PROXY_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_endpoint.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {

class ChannelFactory;
class MessageFilter;
class MessageFilterRouter;
class SendCallbackHelper;

//-----------------------------------------------------------------------------
// IPC::ChannelProxy
//
// This class is a helper class that is useful when you wish to run an IPC
// channel on a background thread.  It provides you with the option of either
// handling IPC messages on that background thread or having them dispatched to
// your main thread (the thread on which the IPC::ChannelProxy is created).
//
// The API for an IPC::ChannelProxy is very similar to that of an IPC::Channel.
// When you send a message to an IPC::ChannelProxy, the message is routed to
// the background thread, where it is then passed to the IPC::Channel's Send
// method.  This means that you can send a message from your thread and your
// message will be sent over the IPC channel when possible instead of being
// delayed until your thread returns to its message loop.  (Often IPC messages
// will queue up on the IPC::Channel when there is a lot of traffic, and the
// channel will not get cycles to flush its message queue until the thread, on
// which it is running, returns to its message loop.)
//
// An IPC::ChannelProxy can have a MessageFilter associated with it, which will
// be notified of incoming messages on the IPC::Channel's thread.  This gives
// the consumer of IPC::ChannelProxy the ability to respond to incoming
// messages on this background thread instead of on their own thread, which may
// be bogged down with other processing.  The result can be greatly improved
// latency for messages that can be handled on a background thread.
//
// The consumer of IPC::ChannelProxy is responsible for allocating the Thread
// instance where the IPC::Channel will be created and operated.
//
// Thread-safe send
//
// If a particular |Channel| implementation has a thread-safe |Send()| operation
// then ChannelProxy skips the inter-thread hop and calls |Send()| directly. In
// this case the |channel_| variable is touched by multiple threads so
// |channel_lifetime_lock_| is used to protect it. The locking overhead is only
// paid if the underlying channel supports thread-safe |Send|.
//
class IPC_EXPORT ChannelProxy : public Endpoint, public base::NonThreadSafe {
 public:
#if defined(ENABLE_IPC_FUZZER)
  // Interface for a filter to be imposed on outgoing messages which can
  // re-write the message. Used for testing.
  class OutgoingMessageFilter {
   public:
    virtual Message* Rewrite(Message* message) = 0;
  };
#endif

  // Initializes a channel proxy.  The channel_handle and mode parameters are
  // passed directly to the underlying IPC::Channel.  The listener is called on
  // the thread that creates the ChannelProxy.  The filter's OnMessageReceived
  // method is called on the thread where the IPC::Channel is running.  The
  // filter may be null if the consumer is not interested in handling messages
  // on the background thread.  Any message not handled by the filter will be
  // dispatched to the listener.  The given task runner correspond to a thread
  // on which IPC::Channel is created and used (e.g. IO thread).
  static std::unique_ptr<ChannelProxy> Create(
      const IPC::ChannelHandle& channel_handle,
      Channel::Mode mode,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner);

  static std::unique_ptr<ChannelProxy> Create(
      std::unique_ptr<ChannelFactory> factory,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner);

  // Constructs a ChannelProxy without initializing it.
  ChannelProxy(
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner);

  ~ChannelProxy() override;

  // Initializes the channel proxy. Only call this once to initialize a channel
  // proxy that was not initialized in its constructor. If create_pipe_now is
  // true, the pipe is created synchronously. Otherwise it's created on the IO
  // thread.
  void Init(const IPC::ChannelHandle& channel_handle,
            Channel::Mode mode,
            bool create_pipe_now);
  void Init(std::unique_ptr<ChannelFactory> factory, bool create_pipe_now);

  // Close the IPC::Channel.  This operation completes asynchronously, once the
  // background thread processes the command to close the channel.  It is ok to
  // call this method multiple times.  Redundant calls are ignored.
  //
  // WARNING: MessageFilter objects held by the ChannelProxy is also
  // released asynchronously, and it may in fact have its final reference
  // released on the background thread.  The caller should be careful to deal
  // with / allow for this possibility.
  void Close();

  // DEPRECATED: Please use either SendNow or SendOnIPCThread to make ordering
  // expectations explicit.
  //
  // This is an alias for for SendOnIPCThread.
  bool Send(Message* message) override;

  // Send a message as soon as possible. This method may send the message
  // immediately, or it may defer and send on the IPC thread. Use this when you
  // you don't care about strict ordering of the send operation with respect to
  // tasks on the IPC thread. This is most commonly what you want.
  virtual bool SendNow(std::unique_ptr<Message> message);

  // Send a message from the IPC thread. This immediately posts a task to the
  // IPC thread task runner to send the message. Use this when you're posting
  // other related tasks to the IPC thread and you need to guarantee that the
  // send operation is ordered with respect to those tasks.
  virtual bool SendOnIPCThread(std::unique_ptr<Message> message);

  // Used to intercept messages as they are received on the background thread.
  //
  // Ordinarily, messages sent to the ChannelProxy are routed to the matching
  // listener on the worker thread.  This API allows code to intercept messages
  // before they are sent to the worker thread.
  // If you call this before the target process is launched, then you're
  // guaranteed to not miss any messages.  But if you call this anytime after,
  // then some messages might be missed since the filter is added internally on
  // the IO thread.
  void AddFilter(MessageFilter* filter);
  void RemoveFilter(MessageFilter* filter);

#if defined(ENABLE_IPC_FUZZER)
  void set_outgoing_message_filter(OutgoingMessageFilter* filter) {
    outgoing_message_filter_ = filter;
  }
#endif

  // Called to clear the pointer to the IPC task runner when it's going away.
  void ClearIPCTaskRunner();

  // Endpoint overrides.
  base::ProcessId GetPeerPID() const override;
  void OnSetAttachmentBrokerEndpoint() override;

#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
  // Calls through to the underlying channel's methods.
  int GetClientFileDescriptor();
  base::ScopedFD TakeClientFileDescriptor();
#endif

 protected:
  class Context;
  // A subclass uses this constructor if it needs to add more information
  // to the internal state.
  ChannelProxy(Context* context);


  // Used internally to hold state that is referenced on the IPC thread.
  class Context : public base::RefCountedThreadSafe<Context>,
                  public Listener {
   public:
    Context(Listener* listener,
            const scoped_refptr<base::SingleThreadTaskRunner>& ipc_thread);
    void ClearIPCTaskRunner();
    base::SingleThreadTaskRunner* ipc_task_runner() const {
      return ipc_task_runner_.get();
    }
    const std::string& channel_id() const { return channel_id_; }

    // Dispatches a message on the listener thread.
    void OnDispatchMessage(const Message& message);

    // Sends |message| from appropriate thread.
    bool Send(std::unique_ptr<Message> message, bool force_io_thread);

    // Indicates if the underlying channel's Send is thread-safe.
    bool IsChannelSendThreadSafe() const;

   protected:
    friend class base::RefCountedThreadSafe<Context>;
    ~Context() override;

    // IPC::Listener methods:
    bool OnMessageReceived(const Message& message) override;
    void OnChannelConnected(int32_t peer_pid) override;
    void OnChannelError() override;

    // Like OnMessageReceived but doesn't try the filters.
    bool OnMessageReceivedNoFilter(const Message& message);

    // Gives the filters a chance at processing |message|.
    // Returns true if the message was processed, false otherwise.
    bool TryFilters(const Message& message);

    // Like Open and Close, but called on the IPC thread.
    virtual void OnChannelOpened();
    virtual void OnChannelClosed();

    // Called on the consumers thread when the ChannelProxy is closed.  At that
    // point the consumer is telling us that they don't want to receive any
    // more messages, so we honor that wish by forgetting them!
    virtual void Clear();

   private:
    friend class ChannelProxy;
    friend class IpcSecurityTestUtil;

    // Create the Channel
    void CreateChannel(std::unique_ptr<ChannelFactory> factory);

    void set_attachment_broker_endpoint(bool is_endpoint) {
      attachment_broker_endpoint_ = is_endpoint;
      if (channel_)
        channel_->SetAttachmentBrokerEndpoint(is_endpoint);
    }

    // Methods called on the IO thread.
    void OnSendMessage(std::unique_ptr<Message> message_ptr);
    void OnAddFilter();
    void OnRemoveFilter(MessageFilter* filter);

    // Methods called on the listener thread.
    void AddFilter(MessageFilter* filter);
    void OnDispatchConnected();
    void OnDispatchError();
    void OnDispatchBadMessage(const Message& message);

    void ClearChannel();

    scoped_refptr<base::SingleThreadTaskRunner> listener_task_runner_;
    Listener* listener_;

    // List of filters.  This is only accessed on the IPC thread.
    std::vector<scoped_refptr<MessageFilter> > filters_;
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

    // Note, channel_ may be set on the Listener thread or the IPC thread.
    // But once it has been set, it must only be read or cleared on the IPC
    // thread.
    // One exception is the thread-safe send. See the class comment.
    std::unique_ptr<Channel> channel_;
    std::string channel_id_;
    bool channel_connected_called_;

    // Lock for |channel_| value. This is only relevant in the context of
    // thread-safe send.
    base::Lock channel_lifetime_lock_;
    // Indicates the thread-safe send availability. This is constant once
    // |channel_| is set.
    bool channel_send_thread_safe_;

    // Routes a given message to a proper subset of |filters_|, depending
    // on which message classes a filter might support.
    std::unique_ptr<MessageFilterRouter> message_filter_router_;

    // Holds filters between the AddFilter call on the listerner thread and the
    // IPC thread when they're added to filters_.
    std::vector<scoped_refptr<MessageFilter> > pending_filters_;
    // Lock for pending_filters_.
    base::Lock pending_filters_lock_;

    // Cached copy of the peer process ID. Set on IPC but read on both IPC and
    // listener threads.
    base::ProcessId peer_pid_;

    // Whether this channel is used as an endpoint for sending and receiving
    // brokerable attachment messages to/from the broker process.
    bool attachment_broker_endpoint_;
  };

  Context* context() { return context_.get(); }

#if defined(ENABLE_IPC_FUZZER)
  OutgoingMessageFilter* outgoing_message_filter() const {
    return outgoing_message_filter_;
  }
#endif

 protected:
  bool did_init() const { return did_init_; }

 private:
  friend class IpcSecurityTestUtil;

  // Always called once immediately after Init.
  virtual void OnChannelInit();

  bool SendImpl(std::unique_ptr<Message> message, bool force_io_thread);

  // By maintaining this indirection (ref-counted) to our internal state, we
  // can safely be destroyed while the background thread continues to do stuff
  // that involves this data.
  scoped_refptr<Context> context_;

  // Whether the channel has been initialized.
  bool did_init_;

#if defined(ENABLE_IPC_FUZZER)
  OutgoingMessageFilter* outgoing_message_filter_;
#endif
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_PROXY_H_
