/*                                                                  -*- c++ -*-
 * Copyright © 2018-2021 Ron R Wills <ron@digitalcombine.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "autocomplete.h"
#include <algorithm>

/******************************************************************************
 * class autocomplete
 */

/******************************
 * autocomplete::autocomplete *
 ******************************/

autocomplete::autocomplete() {
  _lists.push_back(&_list);
}

/*******************************
 * autocomplete::~autocomplete *
 *******************************/

autocomplete::~autocomplete() noexcept {}

/*********************
 * autocomplete::add *
 *********************/

void autocomplete::add(const std::string &val) {
  _list.push_back(val);
}

void autocomplete::add(std::list<std::string> &list) {
  _lists.push_back(&list);
}

/*******************************
 * autocomplete::~autocomplete *
 *******************************/

std::string autocomplete::operator()(const std::string &prefix,
                                     std::string &suggest,
                                     bool &is_more) {
  size_t len = prefix.size();
  std::string answer;
  std::list<std::string>::iterator result;
  bool is_first = true;
  is_more = false;

  // Iterator through all the completion lists.
  for (auto &list: _lists) {

    // Searching for matches in this list.
    result = list->begin();
    while ((result = find_if(result, list->end(),
                            [len, prefix](const std::string &val) {
                              return (val.compare(0, len, prefix) == 0);
                             })) != list->end()) {
      if (is_first) {
        // Our first match.
        answer = *result;
        suggest = *result;
        is_first = false;
        continue;
      }

      // We found more than one match.
      is_more = true;

      // Find the as much unique matching as possible.
      std::pair<std::string::iterator, std::string::iterator> res;

      res = std::mismatch(result->begin(), result->end(),
                          answer.begin());
      if (res.second != answer.end()) {
        answer.erase(res.second, answer.end());
      }
      result++;
    }
  }

  // Return an answer.
  if (answer.empty()) answer = prefix;
  return answer;
}
