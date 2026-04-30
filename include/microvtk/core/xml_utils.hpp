#pragma once

#include <concepts>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace microvtk::core {

class XmlBuilder {
public:
  explicit XmlBuilder(std::ostream& os, int indent_spaces = 2)
      : os_(os), indent_spaces_(indent_spaces) {}

  // Starts a new element: <Name
  void startElement(std::string_view name) {
    if (state_ == State::InStartTag) {
      os_ << ">";
      if (pretty_) os_ << "\n";
    }

    if (pretty_ && state_ != State::Initial) {
      indent();
    }

    os_ << std::format("<{}", name);
    element_stack_.emplace_back(name);
    state_ = State::InStartTag;
    depth_++;
  }

  // Adds an attribute: key="value"
  void attribute(std::string_view name, std::string_view value) {
    os_ << std::format(" {}=\"{}\"", name, escapeXml(value));
  }

  void attribute(std::string_view name, const std::string& value) {
    attribute(name, std::string_view(value));
  }

  void attribute(std::string_view name, const char* value) {
    attribute(name, std::string_view(value));
  }

  // Adds an attribute with numeric value
  template <typename T>
    requires(!std::is_convertible_v<T, std::string_view>)
  void attribute(std::string_view name, const T& value) {
    os_ << std::format(" {}=\"{}\"", name, value);
  }

  // Closes the current element
  void endElement() {
    if (element_stack_.empty()) return;

    std::string name = std::move(element_stack_.back());
    element_stack_.pop_back();
    depth_--;

    if (state_ == State::InStartTag) {
      // Self-closing: />
      os_ << "/>";
      if (pretty_) os_ << "\n";
    } else {
      // Normal closing: </Name>
      if (pretty_) indent();
      os_ << std::format("</{}>", name);
      if (pretty_) os_ << "\n";
    }
    state_ = State::Content;
  }

  // Writes raw string directly to stream (bypass XML formatting)
  void writeRaw(std::string_view str) { os_ << str; }

  // Opens the current start tag so callers can stream raw element content.
  void startRawContent() {
    if (state_ == State::InStartTag) {
      os_ << ">";
      state_ = State::Content;
    }
  }

  // Helper for RAII-style element closing
  class ScopedElement {
    XmlBuilder& builder_;

  public:
    ScopedElement(XmlBuilder& builder, std::string_view name)
        : builder_(builder) {
      builder_.startElement(name);
    }
    ~ScopedElement() { builder_.endElement(); }

    template <typename T>
    ScopedElement& attr(std::string_view name, T&& value) {
      builder_.attribute(name, std::forward<T>(value));
      return *this;
    }
  };

  ScopedElement scopedElement(std::string_view name) { return {*this, name}; }

private:
  static std::string escapeXml(std::string_view str) {
    std::string escaped;
    escaped.reserve(str.size());
    for (char c : str) {
      switch (c) {
        case '&':
          escaped += "&amp;";
          break;
        case '<':
          escaped += "&lt;";
          break;
        case '>':
          escaped += "&gt;";
          break;
        case '\"':
          escaped += "&quot;";
          break;
        case '\'':
          escaped += "&apos;";
          break;
        default:
          escaped += c;
      }
    }
    return escaped;
  }

  void indent() {
    for (int i = 0; i < depth_ * indent_spaces_; ++i) {
      os_ << " ";
    }
  }

  enum class State : std::uint8_t { Initial, InStartTag, Content };

  std::ostream& os_;
  int indent_spaces_;
  std::vector<std::string> element_stack_;
  State state_ = State::Initial;
  int depth_ = 0;
  bool pretty_ = true;
};

}  // namespace microvtk::core
