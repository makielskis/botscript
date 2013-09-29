// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef PACKAGE_H_
#define PACKAGE_H_

#include <string>
#include <vector>
#include <map>

namespace botscript {

/// Abstract parent class for bot package provider classes.
class package {
 public:
  /// \param package_name        the name of the package
  /// \param modules             a mapping from module name (base, server, etc.)
  ///                            to a (possibly gzipped) lua executable
  /// \param zipped              flag indicating whether the modules are gzipped
  /// \throws runtime_exception  if base or server are missing or empty
  package(const std::string& package_name,
          std::map<std::string, std::string> modules,
          bool zipped);

  /// \param url the URL to get the short tag for
  /// \return the short tag for the specified URL
  ///         (or the URL itself if not found)
  const std::string& tag(const std::string& url) const;

  /// \return the modules
  const std::map<std::string, std::string>& modules() const;

  /// \return the interface description
  const std::string& interface() const;

  /// Loads all module files ("*.lua") from the specified folder.
  /// Excludes hidden files (starting with a ".").
  ///
  /// \param p the folder path
  /// \return the loaded modules
  static std::map<std::string, std::string> from_folder(const std::string& p);

  /// Loads the modules contained in the shared library
  /// located at the specified path.
  ///
  /// \param p  the path where shared library is located
  /// \return the loaded modules
  static std::map<std::string, std::string> from_lib(const std::string& p);

 private:
  /// Decompresses using G(un)zip.
  /// \param data the data to decompress
  /// \return the uncompressed data
  static std::vector<char> unzip(const std::vector<char>& data);

  /// Servers mapping (URL -> server short tag).
  std::map<std::string, std::string> servers_;

  /// Modules mapping (module name -> module code)
  std::map<std::string, std::string> modules_;

  /// Package interface description.
  std::string interface_;
};

}  // namespace botscript

#endif  // PACKAGE_H_
