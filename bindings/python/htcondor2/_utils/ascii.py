#!/usr/bin/python3

# Copyright 2025 HTCondor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from enum import IntEnum

class Ascii(IntEnum):
    # Full Name Aliases
    BACKSPACE = 8
    NEWLINE = 10
    CARRIAGE_RETURN = 13
    ESCAPE = 27
    SPACE = 32

    # ASCII table representations for special characters (and variants)
    NULL = 0 # (full)
    NUL  = 0
    SOH  = 1 # start of heading
    STX  = 2 # start of text
    ETX  = 3 # end of text
    EOT  = 4 # end of transmission
    ENQ  = 5 # enquiry
    ACK  = 6 # acknowledgement
    BELL = 7 # Bell (full)
    BEL  = 7 # Bell
    BS   = 8 # backspace
    TAB  = 9 # horizontal tab
    HT   = 9 # horizontal tab
    NL   = 10 # new line (custom)
    LF   = 10 # new line
    VT   = 11 # verticle tab
    FF   = 12 # form feed
    CR   = 13 # carriage return
    SO   = 14 # shift out
    SI   = 15 # shift in
    DLE  = 16 # data link escape
    DC1  = 17 # device control 1
    DC2  = 18 # device control 2
    DC3  = 19 # device control 3
    DC4  = 20 # device control 4
    NAK  = 21 # Negative ack.
    SYN  = 22 # synchronous idle
    ETB  = 23 # end of transmission block
    CAN  = 24 # cancel
    EM   = 25 # end of medium
    SUB  = 26 # substitue
    ESC  = 27 # escape
    FS   = 28 # file separator
    GS   = 29 # group separator
    RS   = 30 # record separator
    US   = 31 # unit separator
    SPC  = 32 # space

    # Special character set 1
    EXCLAMATION  = 33
    EXCL         = 33 # abbriviation: EXCLAMATION
    DOUBLE_QUOTE = 34
    DBLQ         = 34 # abbriviation: DOUBLE_QUOTE
    POUND        = 35
    PND          = 35 # abbriviation: POUND
    HASHTAG      = 35 # alternative name: POUND
    DOLLAR       = 36
    PERCENT      = 37
    PRCT         = 37 # abbriviation: PRCT
    AMPERSAND    = 38
    AND          = 38 # alternative name: AMPERSAND
    SINGLE_QUOTE = 39
    SNGLQ        = 39 # abbriviation: SINGLE_QUOTE
    LEFT_PARENTHESIS  = 40
    LPARAN       = 40 # abbriviation: LEFT_PARENTHESIS
    RIGHT_PARANTHESIS = 41
    RPARAN       = 41 # abbriviation: RIGHT_PARANTHESIS
    ASTERIK      = 42
    STAR         = 42 # alternative name: ASTERIK
    PLUS         = 43
    COMMA        = 44
    HYPHEN       = 45
    MINUS        = 45 # alternative name: HYPHEN
    DASH         = 45 # alternative name: HYPHEN
    PERIOD       = 46
    DOT          = 46 # alternative name: PERIOD
    FORWARD_SLASH= 47
    FSLSH        = 47 # abbriviation: FORWARD_SLASH

    # Numbers
    ZERO  = 48
    ONE   = 49
    TWO   = 50
    THREE = 51
    FOUR  = 52
    FIVE  = 53
    SIX   = 54
    SEVEN = 55
    EIGHT = 56
    NINE  = 57

    # Special character set 2
    COLON         = 58
    SEMICOLON     = 59
    LESS_THAN     = 60
    LTHAN         = 60 # Abbriviation: LESS_THAN
    LSTHN         = 60 # Abbriviation: LESS_THAN
    EQUAL         = 61
    EQL           = 61 # Abbriviation: EQUAL
    GREATER_THAN  = 62
    GTHAN         = 62 # Abbriviation: GREATER_THAN
    GTRTHN        = 62 # Abbriviation: GREATER_THAN
    QUESTION_MARK = 63
    QMARK         = 63 # Abbriviation: QUESTION_MARK
    AT            = 64

    # Uppercase letters
    A = 65
    B = 66
    C = 67
    D = 68
    E = 69
    F = 70
    G = 71
    H = 72
    I = 73
    J = 74
    K = 75
    L = 76
    M = 77
    N = 78
    O = 79
    P = 80
    Q = 81
    R = 82
    S = 83
    T = 84
    U = 85
    V = 86
    W = 87
    X = 88
    Y = 89
    Z = 90

    # Special character set 3
    LEFT_SQUARE_BRACKET  = 91
    L_SQR_BRKT           = 91 # Abbriviation: LEFT_SQUARE_BRACKET
    BACKSLASH            = 92
    BSLSH                = 92 # Abbriviation: BACKSLASH
    RIGHT_SQUARE_BRACKET = 93
    R_SQR_BRKT           = 93 # Abbriviation: RIGHT_SQUARE_BRACKET
    CARET                = 94
    UNDERSCORE           = 95
    BACK_TICK            = 96
    BTCK                 = 96 # Abbriviation: BACK_TICK

    # Lowercase letters
    a = 97
    b = 98
    c = 99
    d = 100
    e = 101
    f = 102
    g = 103
    h = 104
    i = 105
    j = 106
    k = 107
    l = 108
    m = 109
    n = 110
    o = 111
    p = 112
    q = 113
    r = 114
    s = 115
    t = 116
    u = 117
    v = 118
    w = 119
    x = 120
    y = 121
    z = 122

    # Special character set 4
    LEFT_CURLY_BRACKET  = 123
    L_CR_BRKT           = 123 # Abbriviation: LEFT_CURLY_BRACKET
    PIPE                = 124
    RIGHT_CURLY_BRACKET = 125 # Abbriviation: RIGHT_CURLY_BRACKET
    R_CR_BRKT           = 125
    TILDE               = 126
    DELETE              = 127
    DEL                 = 127 # Abbriviation: DELETE

    @property
    def oct(self) -> str:
        """Convert Ascii integer value to octal"""
        return oct(self.value)

    @property
    def hex(self) -> str:
        """Convert Ascii integer value to hexidecimal"""
        return hex(self.value)

    @property
    def char(self) -> str:
        """Convert Ascii integer value to string"""
        return chr(self.value)

    def __cmpval__(self, value) -> int:
        """Convert comparison value into integer value"""
        if isinstance(value, str):
            if len(value) == 1:
                return ord(value)
            raise TypeError("Compare value is not a single character")
        elif isinstance(value, int):
            return value
        raise TypeError(f"{type(value)} not supported for comparison")

    def __eq__(self, other) -> bool:
        return self.value == self.__cmpval__(other)

    def __ne__(self, other) -> bool:
        return self.value != self.__cmpval__(other)

    def __gt__(self, other) -> bool:
        return self.value > self.__cmpval__(other)

    def __lt__(self, other) -> bool:
        return self.value < self.__cmpval__(other)

    def __ge__(self, other) -> bool:
        return self.value >= self.__cmpval__(other)

    def __le__(self, other) -> bool:
        return self.value <= self.__cmpval__(other)
