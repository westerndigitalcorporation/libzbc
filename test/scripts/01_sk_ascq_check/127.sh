#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# This file is part of libzbc.
#
# Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (C) 2023, Western Digital. All rights reserved.

# Reserve one OZR resource from max_open, expecting OZR checks to pass
test_zone_type=${ZT_SWP}

. scripts/01_sk_ascq_check/022.sh "$@"
