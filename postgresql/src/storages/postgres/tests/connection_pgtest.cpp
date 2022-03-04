#include <storages/postgres/tests/util_pgtest.hpp>

#include <userver/engine/single_consumer_event.hpp>

#include <storages/postgres/detail/connection.hpp>
#include <userver/storages/postgres/dsn.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/null.hpp>

USERVER_NAMESPACE_BEGIN

namespace pg = storages::postgres;

namespace static_test {

struct no_input_operator {};
static_assert(pg::io::traits::HasInputOperator<no_input_operator>::value ==
                  false,
              "Test input metafunction");
static_assert(pg::io::traits::HasInputOperator<int>::value,
              "Test input metafunction");
static_assert(!pg::io::traits::kHasParser<no_input_operator>,
              "Test has parser metafunction");
static_assert(pg::io::traits::kHasParser<int>, "Test has parser metafunction");

}  // namespace static_test

namespace {

UTEST_F(PostgreConnection, SelectOne) {
  CheckConnection(conn);

  pg::ResultSet res{nullptr};
  EXPECT_NO_THROW(res = conn->Execute("select 1 as val"))
      << "select 1 successfully executes";
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  EXPECT_FALSE(res.IsEmpty()) << "Result set is obtained";
  EXPECT_EQ(1, res.Size()) << "Result contains 1 row";
  EXPECT_EQ(1, res.FieldCount()) << "Result contains 1 field";

  for (const auto& row : res) {
    EXPECT_EQ(1, row.Size()) << "Row contains 1 field";
    pg::Integer val{0};
    EXPECT_NO_THROW(row.To(val)) << "Extract row data";
    EXPECT_EQ(1, val) << "Correct data extracted";
    EXPECT_NO_THROW(val = row["val"].As<pg::Integer>())
        << "Access field by name";
    EXPECT_EQ(1, val) << "Correct data extracted";
    for (const auto& field : row) {
      EXPECT_FALSE(field.IsNull()) << "Field is not null";
      EXPECT_EQ(1, field.As<pg::Integer>()) << "Correct data extracted";
    }
  }
}

UTEST_F(PostgreConnection, SelectPlaceholder) {
  CheckConnection(conn);

  pg::ResultSet res{nullptr};
  EXPECT_NO_THROW(res = conn->Execute("select $1", 42))
      << "select integral placeholder successfully executes";
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  EXPECT_FALSE(res.IsEmpty()) << "Result set is obtained";
  EXPECT_EQ(1, res.Size()) << "Result contains 1 row";
  EXPECT_EQ(1, res.FieldCount()) << "Result contains 1 field";

  for (const auto& row : res) {
    EXPECT_EQ(1, row.Size()) << "Row contains 1 field";
    for (const auto& field : row) {
      EXPECT_FALSE(field.IsNull()) << "Field is not null";
      EXPECT_EQ(42, field.As<pg::Integer>());
    }
  }

  EXPECT_NO_THROW(res = conn->Execute("select $1", "fooo"))
      << "select text placeholder successfully executes";
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  EXPECT_FALSE(res.IsEmpty()) << "Result set is obtained";
  EXPECT_EQ(1, res.Size()) << "Result contains 1 row";
  EXPECT_EQ(1, res.FieldCount()) << "Result contains 1 field";

  for (const auto& row : res) {
    EXPECT_EQ(1, row.Size()) << "Row contains 1 field";
    for (const auto& field : row) {
      EXPECT_FALSE(field.IsNull()) << "Field is not null";
      EXPECT_EQ("fooo", field.As<std::string>());
    }
  }
}

UTEST_F(PostgreConnection, CheckResultset) {
  CheckConnection(conn);

  pg::ResultSet res{nullptr};
  EXPECT_NO_THROW(res = conn->Execute(
                      "select $1 as str, $2 as int, $3 as float, $4 as double",
                      "foo bar", 42, 3.14f, 6.28))
      << "select four cols successfully executes";
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  EXPECT_FALSE(res.IsEmpty()) << "Result set is obtained";
  EXPECT_EQ(1, res.Size()) << "Result contains 1 row";
  EXPECT_EQ(4, res.FieldCount()) << "Result contains 4 fields";
  EXPECT_EQ(1, res.RowsAffected()) << "The query affected 1 row";
  EXPECT_EQ("SELECT 1", res.CommandStatus());

  for (const auto& row : res) {
    EXPECT_EQ(4, row.Size()) << "Row contains 4 fields";
    {
      std::string str;
      pg::Integer i;
      float f;
      double d;
      EXPECT_NO_THROW(row.To(str, i, f, d));
      EXPECT_EQ("foo bar", str);
      EXPECT_EQ(42, i);
      EXPECT_EQ(3.14f, f);
      EXPECT_EQ(6.28, d);
    }
    {
      std::string str;
      pg::Integer i;
      float f;
      double d;
      EXPECT_NO_THROW(row.To({"int", "str", "double", "float"}, i, str, d, f));
      EXPECT_EQ("foo bar", str);
      EXPECT_EQ(42, i);
      EXPECT_EQ(3.14f, f);
      EXPECT_EQ(6.28, d);
    }
    {
      std::string str;
      pg::Integer i;
      float f;
      double d;
      EXPECT_NO_THROW(row.To({1, 0, 3, 2}, i, str, d, f));
      EXPECT_EQ("foo bar", str);
      EXPECT_EQ(42, i);
      EXPECT_EQ(3.14f, f);
      EXPECT_EQ(6.28, d);
    }
    {
      std::string str;
      pg::Integer i;
      float f;
      double d;
      EXPECT_NO_THROW((std::tie(str, i, f, d) =
                           row.As<std::string, pg::Integer, float, double>()));
      EXPECT_EQ("foo bar", str);
      EXPECT_EQ(42, i);
      EXPECT_EQ(3.14f, f);
      EXPECT_EQ(6.28, d);
    }
    {
      auto [str, i, f, d] = row.As<std::string, pg::Integer, float, double>();
      EXPECT_EQ("foo bar", str);
      EXPECT_EQ(42, i);
      EXPECT_EQ(3.14f, f);
      EXPECT_EQ(6.28, d);
    }
    {
      auto [str, d] = row.As<std::string, double>({"str", "double"});
      EXPECT_EQ("foo bar", str);
      EXPECT_EQ(6.28, d);
    }
    {
      auto [str, d] = row.As<std::string, double>({0, 3});
      EXPECT_EQ("foo bar", str);
      EXPECT_EQ(6.28, d);
    }
  }
}

UTEST_F(PostgreConnection, QueryErrors) {
  CheckConnection(conn);
  pg::ResultSet res{nullptr};
  const std::string temp_table = R"~(
      create temporary table pgtest(
        id integer primary key,
        nn_val integer not null,
        check_val integer check(check_val > 0))
      )~";
  const std::string dependent_table = R"~(
      create temporary table dependent(
        id integer references pgtest(id) on delete restrict)
      )~";
  const std::string insert_pgtest =
      "insert into pgtest(id, nn_val, check_val) values ($1, $2, $3)";

  EXPECT_THROW(res = conn->Execute("elect"), pg::SyntaxError);
  EXPECT_THROW(res = conn->Execute("select foo"), pg::AccessRuleViolation);
  EXPECT_THROW(res = conn->Execute(""), pg::LogicError);

  EXPECT_NO_THROW(conn->Execute(temp_table));
  EXPECT_NO_THROW(conn->Execute(dependent_table));
  EXPECT_THROW(conn->Execute(insert_pgtest, 1, pg::null<int>, pg::null<int>),
               pg::NotNullViolation);
  EXPECT_NO_THROW(conn->Execute(insert_pgtest, 1, 1, pg::null<int>));
  EXPECT_THROW(conn->Execute(insert_pgtest, 1, 1, pg::null<int>),
               pg::UniqueViolation);
  EXPECT_THROW(conn->Execute(insert_pgtest, 2, 1, 0), pg::CheckViolation);
  EXPECT_THROW(conn->Execute("insert into dependent values(3)"),
               pg::ForeignKeyViolation);
  EXPECT_NO_THROW(conn->Execute("insert into dependent values(1)"));
  EXPECT_THROW(conn->Execute("delete from pgtest where id = 1"),
               pg::ForeignKeyViolation);
}

UTEST_F(PostgreConnection, ManualTransaction) {
  CheckConnection(conn);
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  EXPECT_NO_THROW(conn->Execute("begin"))
      << "Successfully execute begin statement";
  EXPECT_EQ(pg::ConnectionState::kTranIdle, conn->GetState());
  EXPECT_NO_THROW(conn->Execute("commit"))
      << "Successfully execute commit statement";
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
}

UTEST_F(PostgreConnection, AutoTransaction) {
  CheckConnection(conn);
  pg::ResultSet res{nullptr};

  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  {
    pg::Transaction trx(std::move(conn), pg::TransactionOptions{});
    // TODO Delegate state to transaction and test it
    //    EXPECT_EQ(pg::ConnectionState::kTranIdle, conn->GetState());
    //    EXPECT_TRUE(conn->IsInTransaction());
    //    EXPECT_THROW(conn->Begin(pg::TransactionOptions{}, cb),
    //                 pg::AlreadyInTransaction);

    EXPECT_NO_THROW(res = trx.Execute("select 1"));
    //    EXPECT_EQ(pg::ConnectionState::kTranIdle, conn->GetState());
    EXPECT_FALSE(res.IsEmpty()) << "Result set is obtained";

    EXPECT_NO_THROW(trx.Commit());

    EXPECT_THROW(trx.Commit(), pg::NotInTransaction);
    EXPECT_NO_THROW(trx.Rollback());
  }
}

UTEST_F(PostgreConnection, RAIITransaction) {
  CheckConnection(conn);
  pg::ResultSet res{nullptr};

  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  {
    pg::Transaction trx(std::move(conn), pg::TransactionOptions{});
    // TODO Delegate state to transaction and test it
    //    EXPECT_EQ(pg::ConnectionState::kTranIdle, conn->GetState());
    //    EXPECT_TRUE(conn->IsInTransaction());

    EXPECT_NO_THROW(res = trx.Execute("select 1"));
    //    EXPECT_EQ(pg::ConnectionState::kTranIdle, conn->GetState());
    EXPECT_FALSE(res.IsEmpty()) << "Result set is obtained";
  }
}

UTEST_F(PostgreConnection, RollbackOnBusyOeErroredConnection) {
  CheckConnection(conn);

  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  // Network timeout
  DefaultCommandControlScope scope(pg::CommandControl{
      std::chrono::milliseconds{10}, std::chrono::milliseconds{0}});
  conn->Begin({}, {});
  EXPECT_THROW(conn->Execute("select pg_sleep(1)"), pg::ConnectionTimeoutError);
  EXPECT_EQ(pg::ConnectionState::kTranActive, conn->GetState());
  EXPECT_NO_THROW(conn->Rollback());
  EXPECT_NO_THROW(conn->CancelAndCleanup(std::chrono::seconds{1}));
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  // Query cancelled
  DefaultCommandControlScope scope2(pg::CommandControl{
      std::chrono::seconds{2}, std::chrono::milliseconds{10}});
  conn->Begin({}, {});
  EXPECT_THROW(conn->Execute("select pg_sleep(1)"), pg::QueryCancelled);
  EXPECT_EQ(pg::ConnectionState::kTranError, conn->GetState());
  EXPECT_NO_THROW(conn->Rollback());
  EXPECT_NO_THROW(conn->CancelAndCleanup(std::chrono::seconds{1}));
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
}

UTEST_F(PostgreConnection, CommitOnBusyOeErroredConnection) {
  CheckConnection(conn);

  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  // Network timeout
  DefaultCommandControlScope scope(pg::CommandControl{
      std::chrono::milliseconds{10}, std::chrono::milliseconds{0}});
  conn->Begin({}, {});
  EXPECT_THROW(conn->Execute("select pg_sleep(1)"), pg::ConnectionTimeoutError);
  EXPECT_EQ(pg::ConnectionState::kTranActive, conn->GetState());
  EXPECT_THROW(conn->Commit(), std::exception);
  EXPECT_NO_THROW(conn->CancelAndCleanup(std::chrono::seconds{1}));
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  // Query cancelled
  DefaultCommandControlScope scope2(pg::CommandControl{
      std::chrono::seconds{2}, std::chrono::milliseconds{10}});
  conn->Begin({}, {});
  EXPECT_THROW(conn->Execute("select pg_sleep(1)"), pg::QueryCancelled);
  EXPECT_EQ(pg::ConnectionState::kTranError, conn->GetState());

  // Server automatically replaces COMMIT with a ROLLBACK for aborted txns
  // TODO: TAXICOMMON-4103
  // EXPECT_THROW(conn->Commit(), std::exception);

  EXPECT_NO_THROW(conn->CancelAndCleanup(std::chrono::seconds{1}));
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
}

UTEST_F(PostgreConnection, StatementTimout) {
  CheckConnection(conn);

  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  // Network timeout
  DefaultCommandControlScope scope(pg::CommandControl{
      std::chrono::milliseconds{10}, std::chrono::milliseconds{0}});
  EXPECT_THROW(conn->Execute("select pg_sleep(1)"), pg::ConnectionTimeoutError);
  EXPECT_EQ(pg::ConnectionState::kTranActive, conn->GetState());
  EXPECT_NO_THROW(conn->CancelAndCleanup(std::chrono::seconds{1}));
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  // Query cancelled
  DefaultCommandControlScope scope2(pg::CommandControl{
      std::chrono::seconds{2}, std::chrono::milliseconds{10}});
  EXPECT_THROW(conn->Execute("select pg_sleep(1)"), pg::QueryCancelled);
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
  EXPECT_NO_THROW(conn->CancelAndCleanup(std::chrono::seconds{1}));
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
}

UTEST_F(PostgreConnection, QueryTaskCancel) {
  CheckConnection(conn);
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());

  DefaultCommandControlScope scope(
      pg::CommandControl{utest::kMaxTestWaitTime, utest::kMaxTestWaitTime});

  engine::SingleConsumerEvent task_started;
  auto task = engine::AsyncNoSpan([&] {
    task_started.Send();
    EXPECT_THROW(conn->Execute("select pg_sleep(1)"),
                 pg::ConnectionInterrupted);
  });
  ASSERT_TRUE(task_started.WaitForEventFor(utest::kMaxTestWaitTime));
  task.RequestCancel();
  task.WaitFor(utest::kMaxTestWaitTime);
  ASSERT_TRUE(task.IsFinished());

  EXPECT_EQ(pg::ConnectionState::kTranActive, conn->GetState());
  EXPECT_NO_THROW(conn->CancelAndCleanup(std::chrono::seconds{1}));
  EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
}

UTEST_F(PostgreConnection, CachedPlanChange) {
  // this only works with english messages, better than nothing
  conn->Execute("SET lc_messages = 'en_US.UTF-8'");
  conn->Execute("CREATE TEMPORARY TABLE plan_change_test ( a integer )");
  EXPECT_NO_THROW(conn->Execute("SELECT * FROM plan_change_test"));
  conn->Execute("ALTER TABLE plan_change_test ALTER a TYPE bigint");
  EXPECT_THROW(conn->Execute("SELECT * FROM plan_change_test"),
               pg::FeatureNotSupported);
  // broken plan should not be reused anymore
  EXPECT_NO_THROW(conn->Execute("SELECT * FROM plan_change_test"));
}

}  // namespace

class PostgreCustomConnection : public PostgreSQLBase {};

UTEST_F(PostgreCustomConnection, Connect) {
  EXPECT_THROW(
      pg::detail::Connection::Connect(
          pg::Dsn{"psql://"}, nullptr, GetTaskProcessor(), kConnectionId,
          kCachePreparedStatements, GetTestCmdCtls(), {}, {}),
      pg::InvalidDSN)
      << "Connected with invalid DSN";

  MakeConnection(GetDsnFromEnv(), GetTaskProcessor());
}

UTEST_F(PostgreCustomConnection, NoPreparedStatements) {
  EXPECT_NO_THROW(pg::detail::Connection::Connect(
      GetDsnFromEnv(), nullptr, GetTaskProcessor(), kConnectionId,
      kNoPreparedStatements, GetTestCmdCtls(), {}, {}));
}

UTEST_F(PostgreCustomConnection, NoUserTypes) {
  std::unique_ptr<pg::detail::Connection> conn;
  EXPECT_NO_THROW(conn = pg::detail::Connection::Connect(
                      GetDsnFromEnv(), nullptr, GetTaskProcessor(),
                      kConnectionId, kNoUserTypes, GetTestCmdCtls(), {}, {}));
  ASSERT_TRUE(conn);

  EXPECT_NO_THROW(conn->Execute("select 1"));
  EXPECT_NO_THROW(conn->Execute("create type user_type as enum ('test')"));
  EXPECT_THROW(conn->Execute("select 'test'::user_type"),
               pg::UnknownBufferCategory);
  EXPECT_NO_THROW(conn->Execute("drop type user_type"));
}

USERVER_NAMESPACE_END
