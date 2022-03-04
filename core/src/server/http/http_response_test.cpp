#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <server/http/http_request_impl.hpp>
#include <userver/engine/async.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/utest/net_listener.hpp>
#include <userver/utest/utest.hpp>

USERVER_NAMESPACE_BEGIN

UTEST(HttpResponse, Smoke) {
  const auto test_deadline =
      engine::Deadline::FromDuration(utest::kMaxTestWaitTime);

  server::request::ResponseDataAccounter accounter;
  server::http::HttpRequestImpl request{accounter};
  server::http::HttpResponse response{request, accounter};

  constexpr std::string_view kBody = "test data";
  response.SetData(std::string{kBody});
  response.SetStatus(server::http::HttpStatus::kOk);

  auto [server, client] = utest::TcpListener{}.MakeSocketPair(test_deadline);
  auto send_task = engine::AsyncNoSpan(
      [](auto&& response, auto&& socket) { response.SendResponse(socket); },
      std::move(response), std::move(server));

  std::vector<char> buffer(4096, '\0');
  const auto reply_size =
      client.RecvAll(buffer.data(), buffer.size(), test_deadline);

  std::string_view reply{buffer.data(), reply_size};
  constexpr std::string_view expected_header = "HTTP/1.1 200 OK\r\n";
  ASSERT_EQ(reply.substr(0, expected_header.size()), expected_header);
  const auto expected_content_length = fmt::format(
      "\r\n{}: {}\r\n", http::headers::kContentLength, kBody.size());
  EXPECT_TRUE(reply.find(expected_content_length) != std::string_view::npos);

  EXPECT_EQ(reply.substr(reply.size() - 4 - kBody.size()),
            fmt::format("\r\n\r\n{}", kBody));
}

UTEST(HttpResponse, ForbiddenBody) {
  const auto test_deadline =
      engine::Deadline::FromDuration(utest::kMaxTestWaitTime);

  server::request::ResponseDataAccounter accounter;
  server::http::HttpRequestImpl request{accounter};
  server::http::HttpResponse response{request, accounter};

  response.SetData("test data");
  response.SetStatus(server::http::HttpStatus::kNoContent);

  auto [server, client] = utest::TcpListener{}.MakeSocketPair(test_deadline);
  auto send_task = engine::AsyncNoSpan(
      [](auto&& response, auto&& socket) { response.SendResponse(socket); },
      std::move(response), std::move(server));

  std::vector<char> buffer(4096, '\0');
  const auto reply_size =
      client.RecvAll(buffer.data(), buffer.size(), test_deadline);

  std::string_view reply{buffer.data(), reply_size};
  constexpr std::string_view expected_header = "HTTP/1.1 204 No Content\r\n";
  ASSERT_EQ(reply.substr(0, expected_header.size()), expected_header);
  EXPECT_TRUE(reply.find(http::headers::kContentLength) ==
              std::string_view::npos);
  EXPECT_EQ(reply.substr(reply.size() - 4), "\r\n\r\n");
}

USERVER_NAMESPACE_END
