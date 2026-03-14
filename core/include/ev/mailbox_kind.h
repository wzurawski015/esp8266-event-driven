#ifndef EV_MAILBOX_KIND_H
#define EV_MAILBOX_KIND_H

/**
 * @brief Supported mailbox semantics for actor inboxes.
 */
typedef enum {
    EV_MAILBOX_FIFO_8 = 0,
    EV_MAILBOX_FIFO_16 = 1,
    EV_MAILBOX_MAILBOX_1 = 2
} ev_mailbox_kind_t;

/**
 * @brief Return a stable textual name for a mailbox kind.
 *
 * @param kind Mailbox kind enumerator.
 * @return Constant string describing the mailbox kind.
 */
const char *ev_mailbox_kind_name(ev_mailbox_kind_t kind);

#endif /* EV_MAILBOX_KIND_H */
