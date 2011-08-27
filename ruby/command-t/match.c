// Copyright 2010 Wincent Colaiuta. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <ctype.h>
#include "match.h"
#include "ext.h"
#include "ruby_compat.h"

// use a struct to make passing params during recursion easier
typedef struct
{
    char    *str_p;                 // pointer to string to be searched
    long    str_len;                // length of same
    char    *abbrev_p;              // pointer to search string (abbreviation)
    long    abbrev_len;             // length of same
    double  max_score_per_char;
    int     dot_file;               // boolean: true if str is a dot-file
    int     always_show_dot_files;  // boolean
    int     never_show_dot_files;   // boolean
} matchinfo_t;

double recursive_match(matchinfo_t *m,  // sharable meta-data
                       long str_idx,    // where in the path string to start
                       long abbrev_idx, // where in the search string to start
                       long last_idx,   // location of last matched character
                       double score)    // cumulative score so far
{
  long i = abbrev_idx;
  long j = str_idx;

  if (i == 0 && score == 0.0) {
    goto match_space;
  }

  char a = m->abbrev_p[i];
  char c = m->str_p[j];

  while (1) {
    if (!a) break;
    if (!c) break;
    // test if c matches a
    if (a == ' ') {
match_space:
      while (m->abbrev_p[i] == ' ') { i++; }
      for ( ; j < m->str_len; j++) {
        double s = recursive_match(m, j, i, 0, 1.0);
        if (s > 0) {
          return 1.0;
        }
      }
      return 0.0;
    }
    if (tolower(a) == tolower(c)) {
      a = m->abbrev_p[++i];
      c = m->str_p[++j];
    } else {
      return 0.0;
    }
  }

  // either a != c or a or c is at end of string
  if (!a) {
    return 1.0; // reached end of pattern - return success
  }
  return 0.0;
}

// Match.new abbrev, string, options = {}
VALUE CommandTMatch_initialize(int argc, VALUE *argv, VALUE self)
{
    // process arguments: 2 mandatory, 1 optional
    VALUE str, abbrev, options;
    if (rb_scan_args(argc, argv, "21", &str, &abbrev, &options) == 2)
        options = Qnil;
    str             = StringValue(str);
    abbrev          = StringValue(abbrev); // already downcased by caller

    // check optional options hash for overrides
    VALUE always_show_dot_files = CommandT_option_from_hash("always_show_dot_files", options);
    VALUE never_show_dot_files = CommandT_option_from_hash("never_show_dot_files", options);

    matchinfo_t m;
    m.str_p                 = RSTRING_PTR(str);
    m.str_len               = RSTRING_LEN(str);
    m.abbrev_p              = RSTRING_PTR(abbrev);
    m.abbrev_len            = RSTRING_LEN(abbrev);
    m.max_score_per_char    = (1.0 / m.str_len + 1.0 / m.abbrev_len) / 2;
    m.dot_file              = 0;
    m.always_show_dot_files = always_show_dot_files == Qtrue;
    m.never_show_dot_files  = never_show_dot_files == Qtrue;

    // calculate score
    double score = 1.0;
    if (m.abbrev_len == 0) // special case for zero-length search string
    {
        // filter out dot files
        if (!m.always_show_dot_files)
        {
            for (long i = 0; i < m.str_len; i++)
            {
                char c = m.str_p[i];
                if (c == '.' && (i == 0 || m.str_p[i - 1] == '/'))
                {
                    score = 0.0;
                    break;
                }
            }
        }
    }
    else // normal case
        score = recursive_match(&m, 0, 0, 0, 0.0);

    // clean-up and final book-keeping
    rb_iv_set(self, "@score", rb_float_new(score));
    rb_iv_set(self, "@str", str);
    return Qnil;
}

VALUE CommandTMatch_matches(VALUE self)
{
    double score = NUM2DBL(rb_iv_get(self, "@score"));
    return score > 0 ? Qtrue : Qfalse;
}

VALUE CommandTMatch_to_s(VALUE self)
{
    return rb_iv_get(self, "@str");
}
