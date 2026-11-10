#pragma once
/// Markdown -> block model for the Thaleia document viewer. The Studio view
/// turns the blocks into ui_kit widgets (no HTML). Covers the CommonMark /
/// GFM subset spec documents use:
///   blocks : ATX headings, paragraphs, fenced code (``` / ~~~), block
///            quotes (nested), bullet / ordered / task lists (nesting by
///            indent), thematic breaks, pipe tables
///   inlines: code spans, **strong** / __strong__, *em* / _em_, ~~strike~~,
///            [links](url), ![images](url), <autolinks>, backslash escapes,
///            hard breaks (two trailing spaces or a trailing backslash)
#include "../common/json_util.h"

#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::musa_thaleia {

enum class InlineKind { Text, Code, Link, Image, Break };

struct Inline {
    InlineKind  kind = InlineKind::Text;
    std::string text;            ///< text / code / link text / image alt
    std::string url;             ///< link / image only
    bool        strong = false;
    bool        em     = false;
    bool        strike = false;
};

enum class BlockKind { Heading, Paragraph, Code, Quote, ListItem, Rule, Table };
enum class Align { None, Left, Center, Right };

struct Block {
    BlockKind           kind = BlockKind::Paragraph;
    int                 level = 0;   ///< heading 1..6 / list nesting depth (0 = top)
    std::vector<Inline> inlines;     ///< heading, paragraph, list item
    std::string         anchor;      ///< heading only (slugify of its text)
    std::string         lang;        ///< code: info string
    std::string         code;        ///< code: body without the fences
    bool                ordered = false;   ///< list item
    int                 number  = 0;       ///< ordered list item
    std::optional<bool> checked;           ///< task list item
    std::vector<Block>  children;          ///< quote
    std::vector<Align>  aligns;            ///< table
    std::vector<std::vector<std::vector<Inline>>> rows;   ///< table, first row = header
};

struct TocEntry {
    int         level = 1;
    std::string title;
    std::string anchor;
};

std::vector<Block>  parse_markdown(const std::string& md);
std::vector<Inline> parse_inlines(const std::string& text);

/// Concatenated text of inlines (breaks become spaces).
std::string plain_text(const std::vector<Inline>& inlines);

/// Heading anchor: tags stripped, ASCII lower-cased, every run of characters
/// other than letters / digits / '-' / '_' (non-ASCII counts as a letter)
/// collapsed to one '-', no leading / trailing '-'.
std::string slugify(const std::string& text);

/// Headings in document order (quotes included).
std::vector<TocEntry> extract_toc(const std::vector<Block>& blocks);
/// Text of the first level-1 heading, if any.
std::optional<std::string> first_h1(const std::vector<Block>& blocks);

JsonValue to_json_value(const std::vector<Block>& blocks);
JsonValue to_json_value(const std::vector<TocEntry>& toc);

} // namespace ergo::studio::plugins::musa_thaleia
