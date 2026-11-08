// =============================================================================
//  seg-display-list.cpp
//
//  A complete SIXTEEN-segment font for the printable ASCII range.  No decimal
//  points, no colon dots: the 16 segments fill the 16 bits of a uint16_t
//  exactly, so one character is still 2 bytes.
//
//  The table is traced from the reference image "16-seg.png" in this folder:
//  the picture holds 96 cells (16 columns x 6 rows = ASCII 32..127) and every
//  cell was sampled segment by segment, so seg_font below is that picture.
//
//  Segment geometry.  The outer frame runs clockwise from the top-left, then
//  the inner star; A and D are split in half, which is what a 16-segment unit
//  adds over a 14-segment one:
//
//          -A1-- --A2-            h : top-left     -> centre
//         | \    |    / |         i : top-centre   -> centre
//         f  h   i   j  b         j : top-right    -> centre
//         |   \  |  /   |         k : centre -> bottom-right
//          -g1-- + --g2-          l : centre -> bottom-centre
//         |   /  |  \   |         m : centre -> bottom-left
//         e  m   l   k  c
//         | /    |    \ |
//          -d1-- --d2-
//
//  Build:
//      g++ -std=c++17 -O2 -Wall -o seg-display-list seg-display-list.cpp
//  Run:
//      seg-display-list                 full listing, 8 glyphs per row
//      seg-display-list --cols 12       wider listing
//      seg-display-list --text "HADES"  render a string
//      seg-display-list --dump          emit the font as a C array
// =============================================================================

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace seg {

// -----------------------------------------------------------------------------
// Segment bit assignment: outer frame clockwise, then the inner star.
// 16 segments -> 16 bits -> exactly 2 bytes per character.
// -----------------------------------------------------------------------------
// Unscoped on purpose: the font table below reads as "a1 | a2 | f | g1" rather
// than carrying a type name on every one of its ~500 terms.  The fixed
// underlying type pins each value to the 16 bits of a glyph.
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

constexpr int first_code  = 32;
constexpr int last_code   = 127;
constexpr int glyph_count = last_code - first_code + 1;  // 96

// -----------------------------------------------------------------------------
// The font, traced from 16-seg.png.
//
// Five entries cannot be copied from the picture because the picture leans on
// the decimal point, or because the picture itself draws two characters
// identically.  They are marked "off-image" and listed again at run time:
//
//   '!'  image = b|c plus the decimal point   -> upper stem i plus the d2 mark
//   '.'  image = decimal point only           -> d2 (where the dot used to sit)
//   '|'  image = i|l, same glyph as ':'        -> b|c (the old '!' body)
//   'x'  image = h|j|k|m, same glyph as 'X'    -> adds the middle bar
//   DEL  image = blank, same glyph as ' '      -> all 16 bits as a filler block
// -----------------------------------------------------------------------------
constexpr std::uint16_t seg_font[glyph_count] = {
    /*  32 ' '  */ 0,
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
    /* 127 DEL  */ 0xFFFF,                                       // off-image
};

// Codes whose pattern is not a literal copy of 16-seg.png, with the reason.
struct off_image_note {
    int code;
    const char* reason;
};

constexpr off_image_note off_image_notes[] = {
    {33,  "image uses b|c plus the decimal point; upper stem i + d2 replaces it"},
    {46,  "image uses the decimal point only; d2 marks the same spot"},
    {124, "image draws '|' exactly like ':'; '|' takes the old '!' bar b|c"},
    {120, "image draws 'x' exactly like 'X'; the middle bar separates them"},
    {127, "image leaves DEL blank like a space; all segments on as a filler"},
};

// -----------------------------------------------------------------------------
// ASCII art: 5 rows x 7 columns per glyph.
// -----------------------------------------------------------------------------
constexpr int art_rows  = 5;
constexpr int art_width = 7;

inline bool is_on(std::uint16_t bits, segment mask) {
    return (bits & mask) != 0;
}

// The centre cell is shared by g1, g2, i, l and the four diagonals.  Pick the
// character that best describes whatever passes through it.
//
// On real hardware the exact centre is dark: all eight inner segments stop
// short of it.  Drawing that dead zone would chop every middle bar in half,
// which reads far worse in seven columns than it looks on glass, so the gap is
// only kept for the one case where it carries meaning: i and l alone, which is
// how ':' tells itself apart from a plain stem.
char centre_cell(std::uint16_t bits) {
    const bool horizontal = is_on(bits, g1) || is_on(bits, g2);
    const bool vertical   = is_on(bits, i) || is_on(bits, l);
    const bool up_left    = is_on(bits, h);
    const bool up_right   = is_on(bits, j);
    const bool down_right = is_on(bits, k);
    const bool down_left  = is_on(bits, m);
    const int  diagonals  = (up_left ? 1 : 0) + (up_right ? 1 : 0) +
                            (down_right ? 1 : 0) + (down_left ? 1 : 0);

    const bool stem_only = is_on(bits, i) && is_on(bits, l) &&
                           !horizontal && diagonals == 0;

    if (horizontal && vertical)          return '+';
    if (horizontal && diagonals == 4)    return 'X';
    if (horizontal && diagonals > 0)     return '+';
    if (horizontal)                      return '-';
    if (stem_only)                       return ' ';   // dead centre
    if (vertical)                        return '|';
    if (diagonals >= 3)                  return 'X';
    if (up_left && up_right)             return 'v';   // both arms from above
    if (down_left && down_right)         return '^';   // both arms from below
    if (up_right && down_right)          return '<';   // both arms from the right
    if (up_left && down_left)            return '>';   // both arms from the left
    if (up_left || down_right)           return '\\';
    if (up_right || down_left)           return '/';
    return ' ';
}

std::array<std::string, art_rows> render_glyph(std::uint16_t bits) {
    std::array<std::string, art_rows> rows;
    for (std::string& row : rows) row.assign(art_width, ' ');

    // split top bar: each half also lights the top-centre joint
    if (is_on(bits, a1)) rows[0][1] = rows[0][2] = rows[0][3] = '-';
    if (is_on(bits, a2)) rows[0][3] = rows[0][4] = rows[0][5] = '-';
    if (is_on(bits, d1)) rows[4][1] = rows[4][2] = rows[4][3] = '-';
    if (is_on(bits, d2)) rows[4][3] = rows[4][4] = rows[4][5] = '-';

    if (is_on(bits, f)) rows[1][0] = '|';
    if (is_on(bits, h)) rows[1][1] = '\\';
    if (is_on(bits, i)) rows[1][3] = '|';
    if (is_on(bits, j)) rows[1][5] = '/';
    if (is_on(bits, b)) rows[1][6] = '|';

    if (is_on(bits, g1)) rows[2][1] = rows[2][2] = '-';
    if (is_on(bits, g2)) rows[2][4] = rows[2][5] = '-';
    rows[2][3] = centre_cell(bits);
    // f/e and b/c touch at mid height: close the side bars so boxes look solid
    if (is_on(bits, f) && is_on(bits, e)) rows[2][0] = '|';
    if (is_on(bits, b) && is_on(bits, c)) rows[2][6] = '|';

    if (is_on(bits, e)) rows[3][0] = '|';
    if (is_on(bits, m)) rows[3][1] = '/';
    if (is_on(bits, l)) rows[3][3] = '|';
    if (is_on(bits, k)) rows[3][5] = '\\';
    if (is_on(bits, c)) rows[3][6] = '|';

    return rows;
}

std::uint16_t pattern_of(int code) {
    if (code < first_code || code > last_code) return 0;
    return seg_font[code - first_code];
}

}  // namespace seg

// -----------------------------------------------------------------------------
// Small text helpers
// -----------------------------------------------------------------------------
static std::string pad_to(std::string text, std::size_t width) {
    if (text.size() < width) text.append(width - text.size(), ' ');
    return text;
}

static std::string label_of(int code) {
    if (code == 32)  return "32 SP";
    if (code == 127) return "127 DEL";
    std::string text = std::to_string(code);
    text += ' ';
    text += static_cast<char>(code);
    return text;
}

static std::string hex_of(std::uint16_t value) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "0x%04X", value);
    return buffer;
}

// Draw a block of glyphs side by side.
static void draw_block(const std::vector<int>& codes, bool with_labels) {
    const std::size_t column = seg::art_width + 2;
    std::string head_line, hex_line;
    std::array<std::string, seg::art_rows> art;

    for (int code : codes) {
        const std::uint16_t bits = seg::pattern_of(code);
        const auto glyph = seg::render_glyph(bits);
        for (int row = 0; row < seg::art_rows; ++row) art[row] += glyph[row] + "  ";
        if (with_labels) {
            head_line += pad_to(label_of(code), column);
            hex_line  += pad_to(hex_of(bits), column);
        }
    }

    if (with_labels) std::printf("%s\n", head_line.c_str());
    for (int row = 0; row < seg::art_rows; ++row) std::printf("%s\n", art[row].c_str());
    if (with_labels) std::printf("%s\n", hex_line.c_str());
}

// -----------------------------------------------------------------------------
// Sections
// -----------------------------------------------------------------------------
static void print_legend() {
    std::puts("=============================================================="
              "=================");
    std::puts(" 16-SEGMENT FONT  -  2 bytes (16 bits) per character, no dots");
    std::puts(" table traced from 16-seg.png (16 columns x 6 rows = ASCII"
              " 32..127)");
    std::puts("=============================================================="
              "=================");
    std::puts("");
    std::puts("   geometry                 bit map");
    std::puts("   -a1-- --a2-              bit  0  a1  top left      bit  8  g1"
              "  middle left");
    std::puts("  | \\    |    / |           bit  1  a2  top right     bit  9  g2"
              "  middle right");
    std::puts("  f  h   i   j  b           bit  2  b   upper right   bit 10  h "
              "  diag top-left  -> centre");
    std::puts("  |   \\  |  /   |           bit  3  c   lower right   bit 11  i "
              "  vert top       -> centre");
    std::puts("   -g1-- + --g2-            bit  4  d2  bottom right  bit 12  j "
              "  diag top-right -> centre");
    std::puts("  |   /  |  \\   |           bit  5  d1  bottom left   bit 13  k "
              "  diag centre -> bottom-right");
    std::puts("  e  m   l   k  c           bit  6  e   lower left    bit 14  l "
              "  vert centre -> bottom");
    std::puts("  | /    |    \\ |           bit  7  f   upper left    bit 15  m "
              "  diag centre -> bottom-left");
    std::puts("   -d1-- --d2-");
    std::puts("");
    std::puts("ASCII art per glyph: 5 rows x 7 columns.");
    std::puts("");
}

static void print_font_table(int columns) {
    std::printf("---- ASCII 32..127 -------------------------------------------"
                "------------------\n\n");
    for (int start = seg::first_code; start <= seg::last_code; start += columns) {
        std::vector<int> codes;
        for (int code = start; code < start + columns && code <= seg::last_code;
             ++code) {
            codes.push_back(code);
        }
        draw_block(codes, true);
        std::printf("\n");
    }
}

static bool print_uniqueness_report() {
    std::map<std::uint16_t, std::vector<int>> buckets;
    for (int code = seg::first_code; code <= seg::last_code; ++code)
        buckets[seg::pattern_of(code)].push_back(code);

    std::size_t collisions = 0;
    for (const auto& bucket : buckets)
        if (bucket.second.size() > 1) ++collisions;

    std::printf("---- uniqueness check ---------------------------------------"
                "-------------------\n");
    std::printf("codes checked          : %d  (ASCII %d..%d)\n",
                seg::glyph_count, seg::first_code, seg::last_code);
    std::printf("distinct 16-bit codes  : %zu\n", buckets.size());

    if (collisions == 0) {
        std::printf("result                 : UNIQUE - every ASCII code maps to"
                    " its own 2-byte pattern\n");
    } else {
        std::printf("result                 : %zu COLLISION(S)\n", collisions);
        for (const auto& bucket : buckets) {
            if (bucket.second.size() < 2) continue;
            std::printf("  %s shared by:", hex_of(bucket.first).c_str());
            for (int code : bucket.second)
                std::printf(" %s", label_of(code).c_str());
            std::printf("\n");
        }
    }
    std::printf("\n");
    return collisions == 0;
}

static void print_image_notes() {
    std::puts("---- where the table leaves 16-seg.png ----------------------"
              "-------------------");
    std::puts("Dropping the two dots costs the image its period and its"
              " exclamation mark, and");
    std::puts("the image itself draws three pairs identically, so five codes"
              " are redrawn:");
    for (const seg::off_image_note& note : seg::off_image_notes)
        std::printf("  %-8s %s\n", label_of(note.code).c_str(), note.reason);
    std::puts("");
    std::puts("'?' keeps its image shape and simply loses the dot; it stays"
              " unique without it.");
    std::puts("The other 91 codes are the picture, segment for segment -"
              " including the details");
    std::puts("that make this font stronger than a 14-segment one: '5' uses"
              " the k diagonal while");
    std::puts("'S' uses the c bar, '0' carries the j|m slash against a plain"
              " 'O', brackets and");
    std::puts("braces are built from the half bars, and the lower-case letters"
              " sit in the lower");
    std::puts("half of the cell.");
    std::puts("");
}

static void print_dump() {
    std::puts("---- font table (2 bytes per character) ---------------------"
              "-------------------");
    std::puts("static const uint16_t seg16_font[96] = {  /* ASCII 32..127 */");
    for (int code = seg::first_code; code <= seg::last_code; code += 8) {
        std::string line = "   ";
        for (int next = code; next < code + 8 && next <= seg::last_code; ++next)
            line += " " + hex_of(seg::pattern_of(next)) + ",";
        line += "   /* " + label_of(code) + " .. */";
        std::printf("%s\n", line.c_str());
    }
    std::puts("};");
    std::puts("");
}

static void print_text(const std::string& text, int columns) {
    std::printf("---- rendered text ------------------------------------------"
                "-------------------\n");
    std::printf("\"%s\"\n\n", text.c_str());
    for (std::size_t start = 0; start < text.size();
         start += static_cast<std::size_t>(columns)) {
        std::vector<int> codes;
        for (std::size_t pos = start;
             pos < start + static_cast<std::size_t>(columns) && pos < text.size();
             ++pos) {
            const int code = static_cast<unsigned char>(text[pos]);
            codes.push_back((code < seg::first_code || code > seg::last_code)
                                ? 32
                                : code);
        }
        draw_block(codes, false);
        std::printf("\n");
    }
}

// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    int columns = 8;
    bool want_dump = false;
    std::string text;

    for (int arg_index = 1; arg_index < argc; ++arg_index) {
        const std::string arg = argv[arg_index];
        if (arg == "--cols" && arg_index + 1 < argc) {
            columns = std::max(1, std::atoi(argv[++arg_index]));
        } else if (arg == "--text" && arg_index + 1 < argc) {
            text = argv[++arg_index];
        } else if (arg == "--dump") {
            want_dump = true;
        } else if (arg == "--help" || arg == "-h") {
            std::puts("usage: seg-display-list [--cols N] [--text STRING]"
                      " [--dump]");
            return 0;
        } else {
            std::printf("unknown argument: %s  (try --help)\n", arg.c_str());
            return 1;
        }
    }

    print_legend();

    if (!text.empty()) {
        print_text(text, columns);
        return 0;
    }

    print_font_table(columns);
    const bool unique = print_uniqueness_report();
    print_image_notes();
    if (want_dump) print_dump();

    return unique ? 0 : 1;
}
