#pragma once

#include <stdlib.h>

#include <unordered_map>
#include <string>

#include <li/http_server/response.hh>

namespace li {

namespace impl {
  inline bool file_exists(const std::string& path) {
    struct stat path_stat;
    return (0 == stat(path.c_str(), &path_stat));
  }

  inline bool is_regular_file(const std::string& path) {
    struct stat path_stat;
    if (-1 == stat(path.c_str(), &path_stat))
      return false;
    return S_ISREG(path_stat.st_mode);
  }

  inline bool is_directory(const std::string& path) {
    struct stat path_stat;
    if (-1 == stat(path.c_str(), &path_stat))
      return false;
    return S_ISDIR(path_stat.st_mode);
  }

  inline bool starts_with(const char *pre, const char *str)
  {
      size_t lenpre = strlen(pre),
            lenstr = strlen(str);
      return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
  }
} // namespace impl

inline auto serve_file(const std::string& root, std::string& path, http_response& response,
                       const std::vector<std::string>& index_files) {
  static char slash = '/';
  static std::unordered_map<std::string, std::string> folderpath_index_map;

  path.erase(path.find_last_not_of(' ') + 1); // Trim off any trailing space.

  std::string full_path(root + path);

  // Check if file exists by real file path.
  char realpath_out[PATH_MAX]{0};
  if (nullptr == realpath(full_path.c_str(), realpath_out)
    || !impl::file_exists(realpath_out))
    throw http_error::not_found("file not found.");

  // Check that path is within the root directory.
  if (!impl::starts_with(root.c_str(), realpath_out))
    throw http_error::forbidden("access denied.");

  // Now we can safely use realpath_out as full_path to do other checks...
  full_path = realpath_out;
  bool is_file(impl::is_regular_file(full_path));
  bool is_directory(impl::is_directory(full_path));

  // If requested is a folder path but the request URL does not have '/' in the end.
  if (is_directory
    && !path.empty() && path.back() != slash)
  {
    response.set_status(301);
    response.set_header("Location", std::string(path) + slash); // BUGBUG: Should be modified based on original request string...
    response.write();
    return;
  }

  // If request URL points to a folder path...
  if (is_directory)
  {
    // ...but no index files configured, no support for listing files.
    if (index_files.empty())
    {
      throw http_error::forbidden("access denied.");
    }

    // ...index file(s) configured, let's use path from request as key to find any index file exists.
    // As value of path from request is shorter than fullpath string, we save a little storage and speed here.
    if (folderpath_index_map.find(std::string(path)) == folderpath_index_map.end()) 
    {
      bool index_file_exists(false);
      for (const auto& index : index_files)
      {
        if (impl::file_exists(full_path + '/' + index))
        {
          folderpath_index_map.emplace(std::string(path), index);
          index_file_exists = true;
          break;
        }
      }
      // Special value empty string to indicate no index file for such folder.
      if (!index_file_exists)
      {
        folderpath_index_map.emplace(std::string(path), "");
      }
    }

    if (folderpath_index_map[std::string(path)].empty())
    {
      throw http_error::forbidden("access denied.");
    }

    full_path = full_path + '/' + folderpath_index_map[std::string(path)];
  } 
  
  // Check if actual requesting path pointing to a regular file.
  // Call is_regular_file again because it might just be composed from index file finding above.
  if (!impl::is_regular_file(full_path))
    throw http_error::not_found("file not found.");
  
  response.write_static_file(full_path);
};

inline auto serve_directory(const std::string& root,
                            const std::vector<std::string>& index_files = {}) {
  // extract root realpath. 
  char realpath_out[PATH_MAX]{0};
  if (nullptr == realpath(root.c_str(), realpath_out))
    throw std::runtime_error(std::string("serve_directory error: Directory ") + root + " does not exist.");

  // Check if it is a directory.
  if (!impl::is_directory(realpath_out))
    throw std::runtime_error(std::string("serve_directory error: ") + root + " is not a directory.");

  http_api api;
  api.get("/{{path...}}") = [realpath_out, index_files](http_request& request, http_response& response) {
    auto path = request.url_parameters(s::path = std::string()).path;
    return serve_file(std::string(realpath_out), path, response, index_files);
  };
  return api;
}

} // namespace li
