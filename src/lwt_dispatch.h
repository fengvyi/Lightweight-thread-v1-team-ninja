#ifndef LWT_DISPATCH_H
#define LWT_DISPATCH_H

struct lwt_context {
	unsigned long ip, sp;
};

void LWT_INLINE __lwt_dispatch(struct lwt_context *curr, struct lwt_context *next);

#endif	/* LWT_DISPATCH_H */
