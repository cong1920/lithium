#include <filesystem>
#include <fstream>
#include <memory>

#include <lithium_http_server.hh>

#include "test.hh"

using namespace li;

int main() {
  namespace fs = std::filesystem;

  char root_tmp[] = "/tmp/webroot_XXXXXX";
  fs::path root(::mkdtemp(root_tmp));
  fs::create_directories(root / "subdir");
  std::unique_ptr<char, void (*)(char*)> tmp_remover(root_tmp,
                                                     [](char* tfp) { fs::remove_all(tfp); });

  {
    std::ofstream ofs((root / "index.html").string());
    ofs << "<html><title>hello</title><body>world</body></html>";
    std::cout << (root / "subdir" / "hello.txt").string() << std::endl;
    std::ofstream ofs2((root / "subdir" / "hello.txt").string());
    ofs2 << "hello world.";
  }

  CHECK_THROW("cannot serve a non existing dir", serve_directory("/xxx"));
  CHECK_THROW("cannot serve a file", serve_directory(root.string() + "/subdir/hello.txt"));

  http_api my_api(serve_directory(root.string(), { "index.html", "hello.txt" }));
  my_api.add_subapi("/test", serve_directory(root.string()));
  http_serve(my_api, 12347, s::non_blocking);
  // http_serve(my_api, 12347);

  CHECK_EQUAL("serve_file returns default index as configured file if requesting root serve path: ",
              http_get("http://localhost:12347").body, "<html><title>hello</title><body>world</body></html>");

  CHECK_EQUAL("serve_file returns default index as configured file if requesting sub folder: ",
              http_get("http://localhost:12347/subdir/").body, "hello world.");

  CHECK_EQUAL("serve_file redirects to the path ending with / if requesting a virtual root directory: ",
              http_get("http://localhost:12347/test").status, 404);         // BUGBUG: 301 with /test/

  auto subfolder_no_slash = http_get("http://localhost:12347/test/subdir", s::fetch_headers);
  CHECK_EQUAL("serve_file redirects to the path ending with / if requesting an actual directory: ",
              subfolder_no_slash.status, 301);
  // BUGBUG: It actually redirects to subdir/\r, missing the virtual folder root test/ and a ghost new line char appened.
  // CHECK_EQUAL("serve_file redirects to the path ending with / if requesting an actual directory: ",
  //            subfolder_no_slash.headers.find("Location")->second, "test/subdir/");

  CHECK_EQUAL("serve_file ok if request out of root because scope is always limited under root: ",
              http_get("http://localhost:12347/..").status, 200);

  // BUGBUG: Not sure what an expected behavior should be... Serving root with no problem sounds right.
  // CHECK_EQUAL("serve_file access denied if out of root of a virtual directory: ",
  //             http_get("http://localhost:12347/test/..").status, 404);

  CHECK_EQUAL("serve_file access forbiden from folder as no index files configured: ", http_get("http://localhost:12347/test/subdir/..").status,
              403);
  CHECK_EQUAL("serve_file not found as no such file: ", http_get("http://localhost:12347/test/subdir/xxx").status,
              404);
  CHECK_EQUAL("serve_file: ", http_get("http://localhost:12347/test/subdir/hello.txt").body,
              "hello world.");
  CHECK_EQUAL("serve_file with .. to locate file relatively: ",
              http_get("http://localhost:12347/test/subdir/../subdir/hello.txt").body,
              "hello world.");
  return 0;
}
