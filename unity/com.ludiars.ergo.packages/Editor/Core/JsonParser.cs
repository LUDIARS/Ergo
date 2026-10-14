// Minimal strict JSON parser (RFC 8259) that records the source span of every value.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace Ludiars.Ergo.Packages
{
    public static class JsonParser
    {
        // Throws FormatException on malformed input.
        public static JsonNode Parse(string text)
        {
            if (text == null) throw new FormatException("JSON text is null");
            var reader = new Reader(text);
            // A UTF-8 BOM decoded into the string is not part of the JSON value.
            if (reader.Pos < text.Length && text[reader.Pos] == '﻿') reader.Pos++;
            JsonNode root = reader.ReadValue();
            reader.SkipWhitespace();
            if (reader.Pos != text.Length) throw reader.Error("unexpected trailing text");
            return root;
        }

        public static bool TryParse(string text, out JsonNode node, out string error)
        {
            try
            {
                node = Parse(text);
                error = null;
                return true;
            }
            catch (FormatException e)
            {
                node = null;
                error = e.Message;
                return false;
            }
        }

        // Encodes s as a JSON string literal (with the quotes).
        public static string Quote(string s)
        {
            var sb = new StringBuilder(s.Length + 2);
            sb.Append('"');
            foreach (char c in s)
            {
                switch (c)
                {
                    case '"': sb.Append("\\\""); break;
                    case '\\': sb.Append("\\\\"); break;
                    case '\n': sb.Append("\\n"); break;
                    case '\r': sb.Append("\\r"); break;
                    case '\t': sb.Append("\\t"); break;
                    default:
                        if (c < 0x20) sb.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture));
                        else sb.Append(c);
                        break;
                }
            }
            sb.Append('"');
            return sb.ToString();
        }

        private sealed class Reader
        {
            private readonly string _text;
            public int Pos;

            public Reader(string text)
            {
                _text = text;
            }

            public FormatException Error(string what)
            {
                return new FormatException("JSON: " + what + " at offset " + Pos.ToString(CultureInfo.InvariantCulture));
            }

            public void SkipWhitespace()
            {
                while (Pos < _text.Length)
                {
                    char c = _text[Pos];
                    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
                    Pos++;
                }
            }

            public JsonNode ReadValue()
            {
                SkipWhitespace();
                if (Pos >= _text.Length) throw Error("unexpected end");
                int start = Pos;
                JsonNode node;
                char c = _text[Pos];
                if (c == '{') node = ReadObject();
                else if (c == '[') node = ReadArray();
                else if (c == '"') node = new JsonNode { Kind = JsonKind.String, StringValue = ReadString() };
                else if (c == 't') node = ReadLiteral("true", new JsonNode { Kind = JsonKind.Bool, BoolValue = true });
                else if (c == 'f') node = ReadLiteral("false", new JsonNode { Kind = JsonKind.Bool, BoolValue = false });
                else if (c == 'n') node = ReadLiteral("null", new JsonNode { Kind = JsonKind.Null });
                else node = ReadNumber();
                node.Start = start;
                node.End = Pos;
                return node;
            }

            private JsonNode ReadLiteral(string word, JsonNode node)
            {
                if (string.CompareOrdinal(_text, Pos, word, 0, word.Length) != 0) throw Error("unexpected token");
                Pos += word.Length;
                return node;
            }

            private JsonNode ReadObject()
            {
                var node = new JsonNode { Kind = JsonKind.Object, Members = new List<KeyValuePair<string, JsonNode>>() };
                Pos++; // {
                SkipWhitespace();
                if (Peek() == '}')
                {
                    Pos++;
                    return node;
                }
                while (true)
                {
                    SkipWhitespace();
                    if (Peek() != '"') throw Error("expected a member name");
                    string key = ReadString();
                    SkipWhitespace();
                    Expect(':');
                    node.Members.Add(new KeyValuePair<string, JsonNode>(key, ReadValue()));
                    SkipWhitespace();
                    char c = Peek();
                    Pos++;
                    if (c == '}') return node;
                    if (c != ',') throw Error("expected ',' or '}'");
                }
            }

            private JsonNode ReadArray()
            {
                var node = new JsonNode { Kind = JsonKind.Array, Items = new List<JsonNode>() };
                Pos++; // [
                SkipWhitespace();
                if (Peek() == ']')
                {
                    Pos++;
                    return node;
                }
                while (true)
                {
                    node.Items.Add(ReadValue());
                    SkipWhitespace();
                    char c = Peek();
                    Pos++;
                    if (c == ']') return node;
                    if (c != ',') throw Error("expected ',' or ']'");
                }
            }

            private string ReadString()
            {
                Pos++; // opening quote
                var sb = new StringBuilder();
                while (true)
                {
                    if (Pos >= _text.Length) throw Error("unterminated string");
                    char c = _text[Pos++];
                    if (c == '"') return sb.ToString();
                    if (c < 0x20) throw Error("control character in string");
                    if (c != '\\')
                    {
                        sb.Append(c);
                        continue;
                    }
                    if (Pos >= _text.Length) throw Error("unterminated escape");
                    char e = _text[Pos++];
                    switch (e)
                    {
                        case '"': sb.Append('"'); break;
                        case '\\': sb.Append('\\'); break;
                        case '/': sb.Append('/'); break;
                        case 'b': sb.Append('\b'); break;
                        case 'f': sb.Append('\f'); break;
                        case 'n': sb.Append('\n'); break;
                        case 'r': sb.Append('\r'); break;
                        case 't': sb.Append('\t'); break;
                        case 'u':
                            if (Pos + 4 > _text.Length) throw Error("short \\u escape");
                            int code;
                            if (!int.TryParse(_text.Substring(Pos, 4), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out code))
                                throw Error("bad \\u escape");
                            sb.Append((char)code);
                            Pos += 4;
                            break;
                        default: throw Error("bad escape");
                    }
                }
            }

            private JsonNode ReadNumber()
            {
                int start = Pos;
                if (Peek() == '-') Pos++;
                while (Pos < _text.Length && "0123456789.eE+-".IndexOf(_text[Pos]) >= 0) Pos++;
                string s = _text.Substring(start, Pos - start);
                double v;
                if (s.Length == 0 || !double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out v))
                {
                    Pos = start;
                    throw Error("unexpected token");
                }
                return new JsonNode { Kind = JsonKind.Number, NumberValue = v };
            }

            private char Peek()
            {
                if (Pos >= _text.Length) throw Error("unexpected end");
                return _text[Pos];
            }

            private void Expect(char c)
            {
                if (Peek() != c) throw Error("expected '" + c + "'");
                Pos++;
            }
        }
    }
}
