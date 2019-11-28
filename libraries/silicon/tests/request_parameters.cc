#include <iod/http_client/http_client.hh>
#include <iod/silicon/silicon.hh>
#include "test.hh"

using namespace iod;

int main() {

  api<http_request, http_response> my_api;

  my_api(GET, "/get") = [&](http_request& request, http_response& response) {
    response.write(json_encode(request.get_parameters(s::id = int())));
  };
  my_api(POST, "/post") = [&](http_request& request, http_response& response) {
    response.write(json_encode(request.post_parameters(s::id = int())));
  };
  my_api(GET, "/url/{{id}}") = [&](http_request& request, http_response& response) {
    response.write(json_encode(request.url_parameters(s::id = int())));
  };

  auto ctx = http_serve(my_api, 12345, s::non_blocking);
  auto ref = json_encode(make_metamap(s::id = 42));

  CHECK_EQUAL("get", http_get("http://localhost:12345/get", s::get_parameters = make_metamap(s::id = 42)).body, ref);
  CHECK_EQUAL("post", http_post("http://localhost:12345/post", s::post_parameters = make_metamap(s::id = 42)).body, ref);
  CHECK_EQUAL("url", http_get("http://localhost:12345/url/42").body, ref);
}
