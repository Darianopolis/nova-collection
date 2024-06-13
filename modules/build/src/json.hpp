#include <bldr.hpp>

#include <ostream>
#include <string>

struct json_writer_t
{
    std::ostream&          out;
    std::string  indent_string = "    ";
    uint32_t             depth = 0;
    bool         first_element = true;
    bool               has_key = false;

public:
    void indent()
    {
        for (uint32_t i = 0; i < depth; ++i) {
            out << indent_string;
        }
    }

    void new_element()
    {
        if (has_key) {
            has_key = false;
            return;
        }

        if (!first_element) {
            out << ',';
        }

        if (depth > 0) {
            out << '\n';
            indent();
        }

        first_element = false;
    }

    json_writer_t& key(std::string_view key)
    {
        new_element();
        out << '"' << key << "\": ";
        has_key = true;
        return *this;
    }

    json_writer_t& operator[](std::string_view key_)
    {
        return key(key_);
    }

    void object()
    {
        new_element();
        out << '{';
        ++depth;
        first_element = true;
    }

    void end_object()
    {
        --depth;
        if (!first_element) {
            out << '\n';
            indent();
        }
        out << '}';
        first_element = false;
    }

    void array()
    {
        new_element();
        out << '[';
        ++depth;
        first_element = true;
    }

    void end_array()
    {
        --depth;
        if (!first_element) {
            out << '\n';
            indent();
        }

        out << ']';
        first_element = false;
    }

    void string(std::string_view str)
    {
        new_element();
        out << '"' << str << '"';
    }

    void operator=(std::string_view str) { string(str); }
    void operator=(char str)             { string({ &str, 1ull }); }
    void operator=(const char* str)      { string(str); }

    template<typename T>
    void value(T&& value)
    {
        new_element();
        out << std::forward<T>(value);
    }

    void operator=(double       v) { value(v);                    }
    void operator=(float        v) { value(v);                    }
    void operator=(int64_t      v) { value(v);                    }
    void operator=(int32_t      v) { value(v);                    }
    void operator=(int16_t      v) { value(v);                    }
    void operator=(int8_t       v) { value(v);                    }
    void operator=(uint64_t     v) { value(v);                    }
    void operator=(uint32_t     v) { value(v);                    }
    void operator=(uint16_t     v) { value(v);                    }
    void operator=(uint8_t      v) { value(v);                    }
    void operator=(bool         v) { value(v ? "true" : "false"); }
    void operator=(std::nullptr_t) { value("null");               }

    template<typename T>
    void operator<<(T&& t)
    {
        this->operator=(std::forward<T>(t));
    }
};