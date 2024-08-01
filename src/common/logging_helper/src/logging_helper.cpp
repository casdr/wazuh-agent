/*
 * Wazuh shared modules utils
 * Copyright (C) 2015-2021, Wazuh Inc.
 * Oct 6, 2021.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include <iostream>
#include "logging_helper.h"

void taggedLogFunction(modules_log_level_t level, const char* log, const char* tag) {

    std::cout << tag << " " << "[" << strlevel[level] << "]: " << log << std::endl;
}
