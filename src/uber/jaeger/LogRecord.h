/*
 * Copyright (c) 2017, Uber Technologies, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef UBER_JAEGER_LOGRECORD_H
#define UBER_JAEGER_LOGRECORD_H

#include <chrono>
#include <string>
#include <vector>

#include <boost/any.hpp>

namespace uber {
namespace jaeger {

class LogRecord {
  public:
    using Clock = std::chrono::steady_clock;

    class Field {
      public:
        using ValueType = boost::any;

        Field() = default;

        template <typename ValueArg>
        Field(const std::string& key, ValueArg&& value)
            : _key(key)
            , _value(std::forward<ValueArg>(value))
        {
        }

        const std::string& key() const { return _key; }

        std::string& key() { return _key; }

        const ValueType& value() const { return _value; }

        ValueType& value() { return _value; }

      private:
        std::string _key;
        ValueType _value;
    };

    template <typename FieldIterator>
    LogRecord(const Clock::time_point& timestamp,
              FieldIterator first,
              FieldIterator last)
        : _timestamp(timestamp)
        , _fields(first, last)
    {
    }

    const Clock::time_point& timestamp() const { return _timestamp; }

    const std::vector<Field>& fields() const { return _fields; }

  private:
    Clock::time_point _timestamp;
    std::vector<Field> _fields;
};

}  // namespace jaeger
}  // namespace uber

#endif  // UBER_JAEGER_LOGRECORD_H
