#ifndef LLARP_LINK_MESSAGE_HPP
#define LLARP_LINK_MESSAGE_HPP

#include <llarp/bencode.hpp>
#include <llarp/router_id.hpp>

#include <queue>
#include <vector>

struct llarp_router;
struct llarp_link_session;
struct llarp_link_session_iter;

namespace llarp
{
  struct ILinkMessage;

  typedef std::queue< ILinkMessage* > SendQueue;

  /// parsed link layer message
  struct ILinkMessage : public IBEncodeMessage
  {
    /// who did this message come from (rc.k)
    RouterID remote  = {};
    uint64_t version = 0;

    ILinkMessage() = default;
    ILinkMessage(const RouterID& id);

    virtual bool
    HandleMessage(llarp_router* router) const = 0;
  };

  struct InboundMessageParser
  {
    InboundMessageParser(llarp_router* router);
    dict_reader reader;

    static bool
    OnKey(dict_reader* r, llarp_buffer_t* buf);

    /// start processig message from a link session
    bool
    ProcessFrom(llarp_link_session* from, llarp_buffer_t buf);

    /// called when the message is fully read
    /// return true when the message was accepted otherwise returns false
    bool
    MessageDone();

   private:
    RouterID
    GetCurrentFrom();

   private:
    bool firstkey;
    llarp_router* router;
    llarp_link_session* from;
    ILinkMessage* msg = nullptr;
  };
}  // namespace llarp

#endif
