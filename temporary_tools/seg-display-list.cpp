//
//          -a1-- --a2-            h : top-left     -> centre
//         | \    |    / |         i : top-centre   -> centre
//         f  h   i   j  b         j : top-right    -> centre
//         |   \  |  /   |         k : centre -> bottom-right
//          -g1-- + --g2-          l : centre -> bottom-centre
//         |   /  |  \   |         m : centre -> bottom-left
//         e  m   l   k  c
//         | /    |    \ |
//          -d1-- --d2-
//
//  Build: g++ -std=c++17 -O2 -Wall -o seg-display-list seg-display-list.cpp

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace seg {

enum segment : std::uint16_t {
    a1 = 1u << 0,   // top bar, left half
    a2 = 1u << 1,   // top bar, right half
    b  = 1u << 2,   // upper right vertical
    c  = 1u << 3,   // lower right vertical
    d2 = 1u << 4,   // bottom bar, right half
    d1 = 1u << 5,   // bottom bar, left half
    e  = 1u << 6,   // lower left vertical
    f  = 1u << 7,   // upper left vertical
    g1 = 1u << 8,   // middle bar, left half
    g2 = 1u << 9,   // middle bar, right half
    h  = 1u << 10,  // diagonal top-left     -> centre
    i  = 1u << 11,  // vertical top-centre   -> centre
    j  = 1u << 12,  // diagonal top-right    -> centre
    k  = 1u << 13,  // diagonal centre -> bottom-right
    l  = 1u << 14,  // vertical centre -> bottom-centre
    m  = 1u << 15,  // diagonal centre -> bottom-left
};

class seg16_display_val {
public:
    constexpr seg16_display_val() = default;

    // a lone segment is already a complete pattern, e.g. '`' is just h
    constexpr seg16_display_val(segment single)
        : bits_(static_cast<std::uint16_t>(single)) {}

    constexpr explicit seg16_display_val(std::uint16_t packed) : bits_(packed) {}

    constexpr seg16_display_val operator|(segment lit) const {
        return seg16_display_val(static_cast<std::uint16_t>(
            bits_ | static_cast<std::uint16_t>(lit)));
    }

    constexpr bool contains(segment lit) const {
        return (bits_ & static_cast<std::uint16_t>(lit)) != 0;
    }

    constexpr std::uint16_t raw() const { return bits_; }

private:
    std::uint16_t bits_ = 0;
};

static_assert(sizeof(seg16_display_val) == 2, "one glyph must stay 2 bytes");

constexpr seg16_display_val operator|(segment lhs, segment rhs) {
    return seg16_display_val(lhs) | rhs;
}

constexpr seg16_display_val no_segments{};
constexpr seg16_display_val all_segments =
    a1 | a2 | b | c | d2 | d1 | e | f | g1 | g2 | h | i | j | k | l | m;

constexpr std::uint8_t first_code  = 32;
constexpr std::uint8_t last_code   = 127;
constexpr std::uint8_t glyph_count = last_code - first_code + 1;  // 96

constexpr seg16_display_val seg_font[glyph_count] = {
    /*  32 ' '  */ no_segments,
    /*  33 '!'  */ i | d2,                                       // off-image
    /*  34 '"'  */ b | i,
    /*  35 '#'  */ b | c | g1 | g2 | i | l | d1 | d2,
    /*  36 '$'  */ a1 | a2 | f | g1 | g2 | c | i | l | d1 | d2,
    /*  37 '%'  */ a1 | f | g1 | i | j | g2 | c | l | m | d2,
    /*  38 '&'  */ a1 | h | i | g1 | e | k | d1 | d2,
    /*  39 '\'' */ i,
    /*  40 '('  */ j | k,
    /*  41 ')'  */ h | m,
    /*  42 '*'  */ g1 | g2 | h | i | j | k | l | m,
    /*  43 '+'  */ g1 | g2 | i | l,
    /*  44 ','  */ m,
    /*  45 '-'  */ g1 | g2,
    /*  46 '.'  */ d2,                                           // off-image
    /*  47 '/'  */ j | m,
    /*  48 '0'  */ a1 | a2 | f | b | e | c | d1 | d2 | j | m,
    /*  49 '1'  */ b | c | j,
    /*  50 '2'  */ a1 | a2 | b | g1 | g2 | e | d1 | d2,
    /*  51 '3'  */ a1 | a2 | b | g2 | c | d1 | d2,
    /*  52 '4'  */ f | b | g1 | g2 | c,
    /*  53 '5'  */ a1 | a2 | f | g1 | k | d1 | d2,
    /*  54 '6'  */ a1 | a2 | f | g1 | g2 | e | c | d1 | d2,
    /*  55 '7'  */ a1 | a2 | b | c,
    /*  56 '8'  */ a1 | a2 | f | b | g1 | g2 | e | c | d1 | d2,
    /*  57 '9'  */ a1 | a2 | f | b | g1 | g2 | c | d1 | d2,
    /*  58 ':'  */ i | l,
    /*  59 ';'  */ i | m,
    /*  60 '<'  */ g1 | j | k,
    /*  61 '='  */ g1 | g2 | d1 | d2,
    /*  62 '>'  */ g2 | h | m,
    /*  63 '?'  */ a1 | a2 | b | g2 | l,
    /*  64 '@'  */ a1 | a2 | f | b | i | g2 | e | d1 | d2,
    /*  65 'A'  */ a1 | a2 | f | b | g1 | g2 | e | c,
    /*  66 'B'  */ a1 | a2 | b | i | g2 | c | l | d1 | d2,
    /*  67 'C'  */ a1 | a2 | f | e | d1 | d2,
    /*  68 'D'  */ a1 | a2 | b | i | c | l | d1 | d2,
    /*  69 'E'  */ a1 | a2 | f | g1 | e | d1 | d2,
    /*  70 'F'  */ a1 | a2 | f | g1 | e,
    /*  71 'G'  */ a1 | a2 | f | g2 | e | c | d1 | d2,
    /*  72 'H'  */ f | b | g1 | g2 | e | c,
    /*  73 'I'  */ a1 | a2 | i | l | d1 | d2,
    /*  74 'J'  */ b | e | c | d1 | d2,
    /*  75 'K'  */ f | g1 | j | e | k,
    /*  76 'L'  */ f | e | d1 | d2,
    /*  77 'M'  */ f | h | j | b | e | c,
    /*  78 'N'  */ f | h | b | e | k | c,
    /*  79 'O'  */ a1 | a2 | f | b | e | c | d1 | d2,
    /*  80 'P'  */ a1 | a2 | f | b | g1 | g2 | e,
    /*  81 'Q'  */ a1 | a2 | f | b | e | c | k | d1 | d2,
    /*  82 'R'  */ a1 | a2 | f | b | g1 | g2 | e | k,
    /*  83 'S'  */ a1 | a2 | f | g1 | g2 | c | d1 | d2,
    /*  84 'T'  */ a1 | a2 | i | l,
    /*  85 'U'  */ f | b | e | c | d1 | d2,
    /*  86 'V'  */ f | e | j | m,
    /*  87 'W'  */ f | b | e | c | k | m,
    /*  88 'X'  */ h | j | k | m,
    /*  89 'Y'  */ f | b | g1 | g2 | c | d1 | d2,
    /*  90 'Z'  */ a1 | a2 | j | m | d1 | d2,
    /*  91 '['  */ a2 | i | l | d2,
    /*  92 '\\' */ h | k,
    /*  93 ']'  */ a1 | i | l | d1,
    /*  94 '^'  */ k | m,
    /*  95 '_'  */ d1 | d2,
    /*  96 '`'  */ h,
    /*  97 'a'  */ g1 | e | l | d1 | d2,
    /*  98 'b'  */ f | g1 | e | l | d1,
    /*  99 'c'  */ g1 | e | d1,
    /* 100 'd'  */ b | g2 | c | l | d2,
    /* 101 'e'  */ g1 | e | m | d1,
    /* 102 'f'  */ a2 | g1 | g2 | i | l,
    /* 103 'g'  */ a1 | f | i | g1 | l | d1,
    /* 104 'h'  */ f | g1 | e | l,
    /* 105 'i'  */ l,
    /* 106 'j'  */ i | l | e | d1,
    /* 107 'k'  */ i | j | k | l,
    /* 108 'l'  */ f | e,
    /* 109 'm'  */ g1 | g2 | e | l | c,
    /* 110 'n'  */ g1 | e | l,
    /* 111 'o'  */ g1 | e | l | d1,
    /* 112 'p'  */ a1 | f | i | g1 | e,
    /* 113 'q'  */ a1 | f | i | g1 | l,
    /* 114 'r'  */ g1 | e,
    /* 115 's'  */ a1 | f | g1 | l | d1,
    /* 116 't'  */ f | g1 | e | d1,
    /* 117 'u'  */ e | l | d1,
    /* 118 'v'  */ e | m,
    /* 119 'w'  */ e | c | k | m,
    /* 120 'x'  */ g1 | g2 | h | j | k | m,                      // off-image
    /* 121 'y'  */ b | i | g2 | c | d2,
    /* 122 'z'  */ g1 | m | d1,
    /* 123 '{'  */ a2 | g1 | i | l | d2,
    /* 124 '|'  */ b | c,                                        // off-image
    /* 125 '}'  */ a1 | g2 | i | l | d1,
    /* 126 '~'  */ g1 | g2 | j | m,
    /* 127 DEL  */ all_segments,                                 // off-image
};

constexpr seg16_display_val pattern_of(std::uint8_t code) {
    if (code < first_code || code > last_code) return no_segments;
    return seg_font[code - first_code];
}

}  // namespace seg


class seg16_ascii_printer {
public:
    explicit seg16_ascii_printer(std::uint8_t glyphs_per_row = 8)
        : glyphs_per_row_(clamped(glyphs_per_row)) {}

    // The whole printable range, every glyph labelled with its code and its
    // 2-byte value.
    void print_font() const {
        for (std::uint8_t start = seg::first_code; start <= seg::last_code;
             start += glyphs_per_row_) {
            print_block(start);
            std::printf("\n");
        }
    }

private:
    static constexpr std::uint8_t art_rows     = 5;  // rows of art per glyph
    static constexpr std::uint8_t art_width    = 7;  // columns of art per glyph
    static constexpr std::uint8_t column_width = art_width + 2;

    // A row never holds more glyphs than the font has, which keeps the 8-bit
    // code arithmetic in print_font clear of its 255 ceiling.
    static constexpr std::uint8_t clamped(std::uint8_t requested) {
        if (requested == 0) return 1;
        return requested > seg::glyph_count ? seg::glyph_count : requested;
    }

    // One band of up to glyphs_per_row_ glyphs: labels, art, 2-byte values.
    void print_block(std::uint8_t start) const {
        std::string label_line, value_line;
        std::array<std::string, art_rows> art;

        for (std::uint8_t code = start;
             code < start + glyphs_per_row_ && code <= seg::last_code; ++code) {
            const seg::seg16_display_val glyph = seg::pattern_of(code);
            const auto rows = render(glyph);
            for (std::uint8_t row = 0; row < art_rows; ++row)
                art[row] += rows[row] + "  ";
            label_line += pad_to(label_of(code), column_width);
            value_line += pad_to(value_of(glyph), column_width);
        }

        std::printf("%s\n", label_line.c_str());
        for (const std::string& row : art) std::printf("%s\n", row.c_str());
        std::printf("%s\n", value_line.c_str());
    }

    static std::array<std::string, art_rows> render(seg::seg16_display_val glyph) {
        std::array<std::string, art_rows> rows;
        for (std::string& row : rows) row.assign(art_width, ' ');

        // each half of the split top and bottom bars also lights the joint
        if (glyph.contains(seg::a1)) rows[0][1] = rows[0][2] = rows[0][3] = '-';
        if (glyph.contains(seg::a2)) rows[0][3] = rows[0][4] = rows[0][5] = '-';
        if (glyph.contains(seg::d1)) rows[4][1] = rows[4][2] = rows[4][3] = '-';
        if (glyph.contains(seg::d2)) rows[4][3] = rows[4][4] = rows[4][5] = '-';

        if (glyph.contains(seg::f)) rows[1][0] = '|';
        if (glyph.contains(seg::h)) rows[1][1] = '\\';
        if (glyph.contains(seg::i)) rows[1][3] = '|';
        if (glyph.contains(seg::j)) rows[1][5] = '/';
        if (glyph.contains(seg::b)) rows[1][6] = '|';

        if (glyph.contains(seg::g1)) rows[2][1] = rows[2][2] = '-';
        if (glyph.contains(seg::g2)) rows[2][4] = rows[2][5] = '-';
        rows[2][3] = centre_cell(glyph);
        // f/e and b/c touch at mid height: close the side bars so boxes look solid
        if (glyph.contains(seg::f) && glyph.contains(seg::e)) rows[2][0] = '|';
        if (glyph.contains(seg::b) && glyph.contains(seg::c)) rows[2][6] = '|';

        if (glyph.contains(seg::e)) rows[3][0] = '|';
        if (glyph.contains(seg::m)) rows[3][1] = '/';
        if (glyph.contains(seg::l)) rows[3][3] = '|';
        if (glyph.contains(seg::k)) rows[3][5] = '\\';
        if (glyph.contains(seg::c)) rows[3][6] = '|';

        return rows;
    }

    static char centre_cell(seg::seg16_display_val glyph) {
        const bool horizontal = glyph.contains(seg::g1) || glyph.contains(seg::g2);
        const bool vertical   = glyph.contains(seg::i) || glyph.contains(seg::l);
        const bool up_left    = glyph.contains(seg::h);
        const bool up_right   = glyph.contains(seg::j);
        const bool down_right = glyph.contains(seg::k);
        const bool down_left  = glyph.contains(seg::m);
        const std::uint8_t diagonals = static_cast<std::uint8_t>(
            (up_left ? 1 : 0) + (up_right ? 1 : 0) +
            (down_right ? 1 : 0) + (down_left ? 1 : 0));
        const bool stem_only  = glyph.contains(seg::i) && glyph.contains(seg::l) &&
                                !horizontal && diagonals == 0;

        if (horizontal && vertical)       return '+';
        if (horizontal && diagonals == 4) return 'X';
        if (horizontal && diagonals > 0)  return '+';
        if (horizontal)                   return '-';
        if (stem_only)                    return ' ';   // dead centre
        if (vertical)                     return '|';
        if (diagonals >= 3)               return 'X';
        if (up_left && up_right)          return 'v';   // both arms from above
        if (down_left && down_right)      return '^';   // both arms from below
        if (up_right && down_right)       return '<';   // both arms from the right
        if (up_left && down_left)         return '>';   // both arms from the left
        if (up_left || down_right)        return '\\';
        if (up_right || down_left)        return '/';
        return ' ';
    }

    // The two ends of the range have no shape of their own to show.
    static std::string label_of(std::uint8_t code) {
        if (code == seg::first_code) return "32 SP";
        if (code == seg::last_code)  return "127 DEL";
        std::string text = std::to_string(static_cast<unsigned>(code));
        text += ' ';
        text += static_cast<char>(code);
        return text;
    }

    static std::string value_of(seg::seg16_display_val glyph) {
        char buffer[16];
        std::snprintf(buffer, sizeof buffer, "0x%04X",
                      static_cast<unsigned>(glyph.raw()));
        return buffer;
    }

    static std::string pad_to(std::string text, std::size_t width) {
        if (text.size() < width) text.append(width - text.size(), ' ');
        return text;
    }

    std::uint8_t glyphs_per_row_;
};

// -----------------------------------------------------------------------------
int main() {
    const seg16_ascii_printer printer;
    printer.print_font();
    return 0;
}
