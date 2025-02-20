#ifndef NODE_CONVERT_H_62B23520_7C8E_11DE_8A39_0800200C9A66
#define NODE_CONVERT_H_62B23520_7C8E_11DE_8A39_0800200C9A66

#if defined(_MSC_VER) ||                                            \
    (defined(__GNUC__) && (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || \
     (__GNUC__ >= 4))  // GCC supports "pragma once" correctly since 3.4
#pragma once
#endif

#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <map>
#include <unordered_map>
#include <sstream>
#include <type_traits>
#include <vector>
#include <stdexcept>

#include "yaml-cpp/binary.h"
#include "yaml-cpp/node/impl.h"
#include "yaml-cpp/node/iterator.h"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/node/type.h"
#include "yaml-cpp/null.h"


namespace YAML {
class Binary;
struct _Null;

template <typename T>
struct convert;
}  // namespace YAML

namespace YAML {
namespace conversion {
inline bool IsInfinity(const std::string& input) {
  return input == ".inf" || input == ".Inf" || input == ".INF" ||
         input == "+.inf" || input == "+.Inf" || input == "+.INF";
}

inline bool IsNegativeInfinity(const std::string& input) {
  return input == "-.inf" || input == "-.Inf" || input == "-.INF";
}

inline bool IsNaN(const std::string& input) {
  return input == ".nan" || input == ".NaN" || input == ".NAN";
}
}

// Node
template <>
struct convert<Node> {
  static Node encode(const Node& rhs) { return rhs; }

  static bool decode(const Node& node, Node& rhs) {
    rhs.reset(node);
    return true;
  }

//  static Node decode(const Node& node) {
//    throw std::runtime_error("this should not have been encountered");
//    Node rhs;
//    rhs.reset(node);
//    return rhs;
//  }
};

// std::string
template <>
struct convert<std::string> {
  static Node encode(const std::string& rhs) { return Node(rhs); }

  static std::string decode(const Node& node) {
    if (!node.IsScalar())
      throw YAML::conversion::DecodeException();
    return node.Scalar();
  }
};

// C-strings can only be encoded
template <>
struct convert<const char*> {
  static Node encode(const char* rhs) { return Node(rhs); }
};

template <>
struct convert<char*> {
  static Node encode(const char* rhs) { return Node(rhs); }
};

template <std::size_t N>
struct convert<char[N]> {
  static Node encode(const char* rhs) { return Node(rhs); }
};

template <>
struct convert<_Null> {
  static Node encode(const _Null& /* rhs */) { return Node(); }

  static _Null decode(const Node& node) {
    if (!node.IsNull())
      throw YAML::conversion::DecodeException();
    return _Null();
  }
};

namespace conversion {
template <typename T>
typename std::enable_if< std::is_floating_point<T>::value, void>::type
inner_encode(const T& rhs, std::stringstream& stream){
  if (std::isnan(rhs)) {
    stream << ".nan";
  } else if (std::isinf(rhs)) {
    if (std::signbit(rhs)) {
      stream << "-.inf";
    } else {
      stream << ".inf";
    }
  } else {
    stream << rhs;
  }
}

template <typename T>
typename std::enable_if<!std::is_floating_point<T>::value, void>::type
inner_encode(const T& rhs, std::stringstream& stream){
  stream << rhs;
}

template <typename T>
typename std::enable_if<(std::is_same<T, unsigned char>::value ||
                         std::is_same<T, signed char>::value), bool>::type
ConvertStreamTo(std::stringstream& stream, T& rhs) {
  int num;
  if ((stream >> std::noskipws >> num) && (stream >> std::ws).eof()) {
    if (num >= (std::numeric_limits<T>::min)() &&
        num <= (std::numeric_limits<T>::max)()) {
      rhs = static_cast<T>(num);
      return true;
    }
  }
  return false;
}

template <typename T>
typename std::enable_if<!(std::is_same<T, unsigned char>::value ||
                          std::is_same<T, signed char>::value), bool>::type
ConvertStreamTo(std::stringstream& stream, T& rhs) {
  if ((stream >> std::noskipws >> rhs) && (stream >> std::ws).eof()) {
    return true;
  }
  return false;
}
}

#define YAML_DEFINE_CONVERT_STREAMABLE(type, negative_op)                  \
  template <>                                                              \
  struct convert<type> {                                                   \
                                                                           \
    static Node encode(const type& rhs) {                                  \
      std::stringstream stream;                                            \
      stream.precision(std::numeric_limits<type>::max_digits10);           \
      conversion::inner_encode(rhs, stream);                               \
      return Node(stream.str());                                           \
    }                                                                      \
                                                                           \
    static type decode(const Node& node) {                                 \
      if (node.Type() != NodeType::Scalar) {                               \
        throw YAML::conversion::DecodeException();;                        \
      }                                                                    \
      const std::string& input = node.Scalar();                            \
      std::stringstream stream(input);                                     \
      stream.unsetf(std::ios::dec);                                        \
      if ((stream.peek() == '-') && std::is_unsigned<type>::value) {       \
        throw YAML::conversion::DecodeException();                         \
      }                                                                    \
      type rhs;                                                            \
      if (conversion::ConvertStreamTo(stream, rhs)) {                      \
        return rhs;                                                        \
      }                                                                    \
      if (std::numeric_limits<type>::has_infinity) {                       \
        if (conversion::IsInfinity(input)) {                               \
          return std::numeric_limits<type>::infinity();                    \
        } else if (conversion::IsNegativeInfinity(input)) {                \
          return negative_op std::numeric_limits<type>::infinity();        \
        }                                                                  \
      }                                                                    \
                                                                           \
      if (std::numeric_limits<type>::has_quiet_NaN) {                      \
        if (conversion::IsNaN(input)) {                                    \
          return std::numeric_limits<type>::quiet_NaN();                   \
        }                                                                  \
      }                                                                    \
                                                                           \
      throw YAML::conversion::DecodeException();                           \
    }                                                                      \
  }

#define YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(type) \
  YAML_DEFINE_CONVERT_STREAMABLE(type, -)

#define YAML_DEFINE_CONVERT_STREAMABLE_UNSIGNED(type) \
  YAML_DEFINE_CONVERT_STREAMABLE(type, +)

YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(int);
YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(short);
YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(long);
YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(long long);
YAML_DEFINE_CONVERT_STREAMABLE_UNSIGNED(unsigned);
YAML_DEFINE_CONVERT_STREAMABLE_UNSIGNED(unsigned short);
YAML_DEFINE_CONVERT_STREAMABLE_UNSIGNED(unsigned long);
YAML_DEFINE_CONVERT_STREAMABLE_UNSIGNED(unsigned long long);

YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(char);
YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(signed char);
YAML_DEFINE_CONVERT_STREAMABLE_UNSIGNED(unsigned char);

YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(float);
YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(double);
YAML_DEFINE_CONVERT_STREAMABLE_SIGNED(long double);

#undef YAML_DEFINE_CONVERT_STREAMABLE_SIGNED
#undef YAML_DEFINE_CONVERT_STREAMABLE_UNSIGNED
#undef YAML_DEFINE_CONVERT_STREAMABLE

// bool
template <>
struct convert<bool> {
  static Node encode(bool rhs) { return rhs ? Node("true") : Node("false"); }

  YAML_CPP_API static bool decode(const Node& node);
};

// std::map
template <typename K, typename V, typename C, typename A>
struct convert<std::map<K, V, C, A>> {
  static Node encode(const std::map<K, V, C, A>& rhs) {
    Node node(NodeType::Map);
    for (const auto& element : rhs)
      node.force_insert(element.first, element.second);
    return node;
  }

  static std::map<K, V, C, A> decode(const Node& node) {
    if (!node.IsMap())
      throw YAML::conversion::DecodeException();

    std::map<K, V, C, A> rhs;
    for (const auto& element : node)
#if defined(__GNUC__) && __GNUC__ < 4
      // workaround for GCC 3:
      rhs[element.first.template as<K>()] = element.second.template as<V>();
#else
      rhs[element.first.as<K>()] = element.second.as<V>();
#endif
    return rhs;
  }
};

// std::unordered_map
template <typename K, typename V, typename H, typename P, typename A>
struct convert<std::unordered_map<K, V, H, P, A>> {
  static Node encode(const std::unordered_map<K, V, H, P, A>& rhs) {
    Node node(NodeType::Map);
    for (const auto& element : rhs)
      node.force_insert(element.first, element.second);
    return node;
  }

  static std::unordered_map<K, V, H, P, A> decode(const Node& node) {
    if (!node.IsMap())
      throw YAML::conversion::DecodeException();

    std::unordered_map<K, V, H, P, A> rhs;
    for (const auto& element : node)
#if defined(__GNUC__) && __GNUC__ < 4
      // workaround for GCC 3:
      rhs[element.first.template as<K>()] = element.second.template as<V>();
#else
      rhs[element.first.as<K>()] = element.second.as<V>();
#endif
    return rhs;
  }
};

// std::vector
template <typename T, typename A>
struct convert<std::vector<T, A>> {
  static Node encode(const std::vector<T, A>& rhs) {
    Node node(NodeType::Sequence);
    for (const auto& element : rhs)
      node.push_back(element);
    return node;
  }

  static std::vector<T, A> decode(const Node& node) {
    if (!node.IsSequence())
      throw YAML::conversion::DecodeException();

    std::vector<T, A> rhs;
    for (const auto& element : node)
#if defined(__GNUC__) && __GNUC__ < 4
      // workaround for GCC 3:
      rhs.push_back(element.template as<T>());
#else
      rhs.push_back(element.as<T>());
#endif
    return rhs;
  }
};

// std::list
template <typename T, typename A>
struct convert<std::list<T,A>> {
  static Node encode(const std::list<T,A>& rhs) {
    Node node(NodeType::Sequence);
    for (const auto& element : rhs)
      node.push_back(element);
    return node;
  }

  static std::list<T,A> decode(const Node& node) {
    if (!node.IsSequence())
      throw YAML::conversion::DecodeException();

    std::list<T,A> rhs;
    for (const auto& element : node)
#if defined(__GNUC__) && __GNUC__ < 4
      // workaround for GCC 3:
      rhs.push_back(element.template as<T>());
#else
      rhs.push_back(element.as<T>());
#endif
    return rhs;
  }
};

// std::array
template <typename T, std::size_t N>
struct convert<std::array<T, N>> {
  static Node encode(const std::array<T, N>& rhs) {
    Node node(NodeType::Sequence);
    for (const auto& element : rhs) {
      node.push_back(element);
    }
    return node;
  }

  static std::array<T, N> decode(const Node& node) {
    if (!isNodeValid(node))
      throw YAML::conversion::DecodeException();

    std::array<T, N> rhs;
    for (auto i = 0u; i < node.size(); ++i) {
#if defined(__GNUC__) && __GNUC__ < 4
      // workaround for GCC 3:
      rhs[i] = node[i].template as<T>();
#else
      rhs[i] = node[i].as<T>();
#endif
    }
    return rhs;
  }

 private:
  static bool isNodeValid(const Node& node) {
    return node.IsSequence() && node.size() == N;
  }
};

// std::pair
template <typename T, typename U>
struct convert<std::pair<T, U>> {
  static Node encode(const std::pair<T, U>& rhs) {
    Node node(NodeType::Sequence);
    node.push_back(rhs.first);
    node.push_back(rhs.second);
    return node;
  }

  static std::pair<T, U> decode(const Node& node) {
    if (!node.IsSequence() || node.size() != 2)
      throw YAML::conversion::DecodeException();

    std::pair<T, U> rhs;
#if defined(__GNUC__) && __GNUC__ < 4
    // workaround for GCC 3:
    rhs.first = node[0].template as<T>();
#else
    rhs.first = node[0].as<T>();
#endif
#if defined(__GNUC__) && __GNUC__ < 4
    // workaround for GCC 3:
    rhs.second = node[1].template as<U>();
#else
    rhs.second = node[1].as<U>();
#endif
    return rhs;
  }
};

// binary
template <>
struct convert<Binary> {
  static Node encode(const Binary& rhs) {
    return Node(EncodeBase64(rhs.data(), rhs.size()));
  }

  static Binary decode(const Node& node) {
    if (!node.IsScalar())
      throw YAML::conversion::DecodeException();

    std::vector<unsigned char> data = DecodeBase64(node.Scalar());
    if (data.empty() && !node.Scalar().empty())
      throw YAML::conversion::DecodeException();

    Binary rhs;
    rhs.swap(data);
    return rhs;
  }
};
}

#endif  // NODE_CONVERT_H_62B23520_7C8E_11DE_8A39_0800200C9A66
