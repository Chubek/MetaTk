/// In-memory source ownership and byte-based, one-based line/column mapping.
module frontend.Input;

import std.exception : enforce;

struct SourceLocation {
    string file;
    size_t line;
    size_t column;
    size_t offset;

    /// Identity is the file and byte offset; line/column are derived views.
    bool opEquals(const SourceLocation other) const {
        return offset == other.offset && file == other.file;
    }
}

/// Half-open byte range. EOF has a zero-width span at text.length.
struct SourceSpan {
    SourceLocation start;
    SourceLocation end;

    bool opEquals(const SourceSpan other) const {
        return start == other.start && end == other.end;
    }
}

/// No filesystem access: reading a file is the driver's responsibility.
final class SourceText {
    immutable string text;
    immutable string name;
    private size_t[] lineStarts;

    this(string text, string name = "<memory>") {
        this.text = text;
        this.name = name;
        lineStarts = [0];
        for (size_t i = 0; i < text.length; ++i) {
            if (text[i] == '\r') {
                if (i + 1 < text.length && text[i + 1] == '\n') ++i;
                lineStarts ~= i + 1;
            } else if (text[i] == '\n') {
                lineStarts ~= i + 1;
            }
        }
    }

    SourceLocation locationAt(size_t offset) const {
        enforce(offset <= text.length, "source offset is outside the source");
        size_t low = 0;
        size_t high = lineStarts.length;
        while (low + 1 < high) {
            const mid = low + (high - low) / 2;
            if (lineStarts[mid] <= offset) low = mid;
            else high = mid;
        }
        return SourceLocation(name, low + 1, offset - lineStarts[low] + 1, offset);
    }

    SourceSpan span(size_t begin, size_t end) const {
        enforce(begin <= end, "source span is reversed");
        return SourceSpan(locationAt(begin), locationAt(end));
    }
}

string moduleName() @safe pure nothrow { return "frontend.Input"; }
