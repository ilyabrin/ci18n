/*
 * Must not compile: "greting" is not a key in stress.txt, and CI18N_KEY is
 * there to turn exactly this into a build error. `make test-compiled` checks
 * that it fails.
 *
 * SPDX-License-Identifier: MIT
 */

#include "stress.h"

const char *typo(void)
{
    return ci18n_get(CI18N_KEY(greting));
}
