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
  /// \param name     the package name
  /// \param modules  the modules
  package(std::string name, std::map<std::string, std::string> modules);

  /// \param package_name        the name of the package
  /// \param modules             a mapping from module name (base, server, etc.)
  ///                            to a (possibly gzipped) lua executable
  /// \param zipped              flag indicating whether the modules are gzipped
  /// \throws runtime_exception  if base or server are missing or empty
  explicit package(const std::string& path);

  /// \return the package name
  const std::string& name() const;

  /// \param url the URL to get the short tag for
  /// \return the short tag for the specified URL
  ///         (or the URL itself if not found)
  const std::string& tag(const std::string& url) const;

  /// \return the modules
  const std::map<std::string, std::string>& modules() const;

  /// \return the interface description
  const std::string& interface_desc() const;

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
  /// \param path  the path to the package (shared object or folder)
  /// \return the filename without ".packages" file ending (if available)
  static std::string name_from_path(const std::string& path);

  /// \throws std::runtime_error if the modules base and/or servers are missing.
  /// \param path  the path to read the modules from (folder or shared object)
  /// \return the contained modules
  static std::map<std::string, std::string> read_modules(const std::string& path);

  /// \param modules  the modules to unzipp if the second path argument
  ///                 is a regular file (no directory)
  /// \return the unzipped modules (if the second arg was a regular file)
  static std::map<std::string, std::string> unzip_if_no_directory(
      std::map<std::string, std::string> modules,
      const std::string& path);

  /// \param modules  the modules to unzipp
  static std::map<std::string, std::string> unzip(
      std::map<std::string, std::string> modules);

  /// Generates a JSON description of the package.
  ///
  /// \param modules       the packages's modules
  /// \param servers       the packages's servers
  /// \param package_name  the package name (e.g. 'kv' or 'pg')
  std::string json_description(
      const std::map<std::string, std::string>& modules,
      const std::map<std::string, std::string>& servers,
      const std::string& package_name);

  /// Decompresses using G(un)zip.
  ///
  /// \param data the data to decompress
  /// \return the uncompressed data
  static std::vector<char> unzip(const std::vector<char>& data);

  /// The name of this package.
  std::string name_;

  /// Modules mapping (module name -> module code)
  std::map<std::string, std::string> modules_;

  /// Servers mapping (URL -> server short tag).
  std::map<std::string, std::string> servers_;

  /// Package interface description.
  std::string interface_;
};

}  // namespace botscript

#endif  // PACKAGE_H_
