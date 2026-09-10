// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Tests for the Responses lifecycle: model-bound continuation and the lease that keeps deletion coherent with
// requests that are already generating.

#include "inferencing/generative/openresponses/response_store.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace fl;
using json = nlohmann::json;

namespace {

/// Records what the store admits, standing in for the service's session cache.
class RecordingAdmission final : public IResponseAdmission {
 public:
  void Admit(const std::string& response_id) override { admitted.push_back(response_id); }
  void Rollback(const std::string& response_id) noexcept override {
    admitted.erase(std::remove(admitted.begin(), admitted.end(), response_id), admitted.end());
  }

  std::vector<std::string> admitted;
};

/// Records what the store drops, standing in for SessionManager::EvictCached.
class RecordingCache final : public IResponseCacheCoordinator {
 public:
  void Drop(const std::string& response_id) override { dropped.push_back(response_id); }

  std::vector<std::string> dropped;
};

class ThrowingAdmission final : public IResponseAdmission {
 public:
  void Admit(const std::string&) override { throw std::runtime_error("cache admission failed"); }
  void Rollback(const std::string&) noexcept override { rollback_called = true; }

  bool rollback_called = false;
};

/// A response object as the handler stores it: only `previous_response_id` and `output` matter to the store.
json ResponseJson(const std::string& id, const std::string& previous_id = "") {
  json response = {{"id", id}, {"output", json::array()}};
  if (!previous_id.empty()) {
    response["previous_response_id"] = previous_id;
  }

  return response;
}

/// Run one complete request through the store the way the handler does, and report whether it was published.
bool RunTurn(ResponseStore& store, const std::string& id, const std::string& previous_id,
             const std::string& model_id, IResponseAdmission* admission = nullptr) {
  auto continuation = store.BeginResponse(previous_id, model_id);
  EXPECT_EQ(continuation.status, ContinuationStatus::kOk);

  return store.Commit(continuation.lease,
                      ResponseStore::StoredResponse{id, model_id, ResponseJson(id, previous_id), json::array()},
                      admission);
}

}  // namespace

// ========================================================================
// Cross-model continuation
// ========================================================================

TEST(ResponseContinuationTest, ContinuingWithTheModelThatProducedTheParentIsAccepted) {
  ResponseStore store;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");

  auto continuation = store.BeginResponse("resp_1", "model-a");

  EXPECT_EQ(continuation.status, ContinuationStatus::kOk);
  EXPECT_TRUE(static_cast<bool>(continuation.lease));
}

TEST(ResponseContinuationTest, ContinuingWithADifferentModelIsRejected) {
  ResponseStore store;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");

  auto continuation = store.BeginResponse("resp_1", "model-b");

  EXPECT_EQ(continuation.status, ContinuationStatus::kModelMismatch);
  EXPECT_EQ(continuation.parent_model_id, "model-a");
  EXPECT_FALSE(static_cast<bool>(continuation.lease));
}

TEST(ResponseContinuationTest, ARejectedCrossModelContinuationHoldsNoLease) {
  ResponseStore store;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");

  auto continuation = store.BeginResponse("resp_1", "model-b");

  EXPECT_EQ(store.InFlightResponses(), 0u);
}

TEST(ResponseContinuationTest, TheModelOfTheImmediateParentDecidesRegardlessOfChainDepth) {
  ResponseStore store;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");
  store.Store("resp_2", ResponseJson("resp_2", "resp_1"), json::array(), "model-a");

  EXPECT_EQ(store.BeginResponse("resp_2", "model-b").status, ContinuationStatus::kModelMismatch);
  EXPECT_EQ(store.BeginResponse("resp_2", "model-a").status, ContinuationStatus::kOk);
}

TEST(ResponseContinuationTest, AnUnknownParentIsReportedAsAnUnavailableChainNotAModelMismatch) {
  ResponseStore store;

  auto continuation = store.BeginResponse("resp_missing", "model-a");

  EXPECT_EQ(continuation.status, ContinuationStatus::kChainUnavailable);
  EXPECT_EQ(store.InFlightResponses(), 0u);
}

TEST(ResponseContinuationTest, ABrokenChainIsReportedBeforeTheModelIsCompared) {
  ResponseStore store;
  // resp_2's parent was never stored, so the conversation cannot be replayed at all.
  store.Store("resp_2", ResponseJson("resp_2", "resp_1"), json::array(), "model-a");

  EXPECT_EQ(store.BeginResponse("resp_2", "model-b").status, ContinuationStatus::kChainUnavailable);
}

TEST(ResponseContinuationTest, AParentStoredWithoutAModelConstrainsNothing) {
  ResponseStore store;
  store.Store("resp_1", ResponseJson("resp_1"), json::array());

  EXPECT_EQ(store.BeginResponse("resp_1", "model-a").status, ContinuationStatus::kOk);
  EXPECT_EQ(store.BeginResponse("resp_1", "model-b").status, ContinuationStatus::kOk);
}

TEST(ResponseContinuationTest, AModelBoundParentDoesNotConstrainACallerThatResolvesNoModel) {
  ResponseStore store;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");

  EXPECT_EQ(store.BeginResponse("resp_1", "").status, ContinuationStatus::kOk);
}

TEST(ResponseContinuationTest, ANewConversationIsAlwaysAccepted) {
  ResponseStore store;

  auto continuation = store.BeginResponse("", "model-a");

  EXPECT_EQ(continuation.status, ContinuationStatus::kOk);
  EXPECT_TRUE(static_cast<bool>(continuation.lease));
}

TEST(ResponseContinuationTest, CommittedResponsesCarryTheirModelToTheNextTurn) {
  ResponseStore store;
  ASSERT_TRUE(RunTurn(store, "resp_1", "", "model-a"));
  ASSERT_TRUE(RunTurn(store, "resp_2", "resp_1", "model-a"));

  EXPECT_EQ(store.BeginResponse("resp_2", "model-a").status, ContinuationStatus::kOk);
  EXPECT_EQ(store.BeginResponse("resp_2", "model-b").status, ContinuationStatus::kModelMismatch);
}

TEST(ResponseContinuationTest, AcceptingAContinuationKeepsTheWholeChainResident) {
  // Capacity 3 with a 3-hop conversation: opening a continuation must refresh every hop, otherwise storing the
  // fourth response evicts the root and the conversation can no longer be rebuilt.
  ResponseStore store(3);
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");
  store.Store("resp_2", ResponseJson("resp_2", "resp_1"), json::array(), "model-a");
  store.Store("resp_3", ResponseJson("resp_3", "resp_2"), json::array(), "model-a");

  auto continuation = store.BeginResponse("resp_3", "model-a");
  ASSERT_EQ(continuation.status, ContinuationStatus::kOk);

  EXPECT_TRUE(store.Get("resp_1").has_value());
  EXPECT_TRUE(store.Get("resp_3").has_value());
}

// ========================================================================
// Lease lifetime
// ========================================================================

TEST(ResponseLeaseTest, AnOpenLeaseIsReleasedWhenItGoesOutOfScope) {
  ResponseStore store;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");

  {
    auto continuation = store.BeginResponse("resp_1", "model-a");
    EXPECT_EQ(store.InFlightResponses(), 1u);
  }

  EXPECT_EQ(store.InFlightResponses(), 0u);
}

TEST(ResponseLeaseTest, CommittingReleasesTheLease) {
  ResponseStore store;

  auto continuation = store.BeginResponse("", "model-a");
  ASSERT_TRUE(store.Commit(continuation.lease,
                           ResponseStore::StoredResponse{"resp_1", "model-a", ResponseJson("resp_1"), json::array()},
                           nullptr));

  EXPECT_EQ(store.InFlightResponses(), 0u);
  EXPECT_FALSE(static_cast<bool>(continuation.lease));
}

TEST(ResponseLeaseTest, MovingALeaseTransfersTheSingleRegistration) {
  ResponseStore store;

  auto continuation = store.BeginResponse("", "model-a");
  ResponseLease moved = std::move(continuation.lease);

  EXPECT_FALSE(static_cast<bool>(continuation.lease));
  EXPECT_TRUE(static_cast<bool>(moved));
  EXPECT_EQ(store.InFlightResponses(), 1u);

  moved.Release();
  EXPECT_EQ(store.InFlightResponses(), 0u);
}

TEST(ResponseLeaseTest, ReleaseIsIdempotent) {
  ResponseStore store;

  auto continuation = store.BeginResponse("", "model-a");
  continuation.lease.Release();
  continuation.lease.Release();

  EXPECT_EQ(store.InFlightResponses(), 0u);
}

TEST(ResponseLeaseTest, CommittingWithoutALeaseStoresNothing) {
  ResponseStore store;
  ResponseLease empty;
  RecordingAdmission admission;

  EXPECT_FALSE(store.Commit(
      empty, ResponseStore::StoredResponse{"resp_1", "model-a", ResponseJson("resp_1"), json::array()}, &admission));
  EXPECT_FALSE(store.Get("resp_1").has_value());
  EXPECT_TRUE(admission.admitted.empty());
}

TEST(ResponseLeaseTest, CommittingALeaseFromAnotherStoreLeavesThatLeaseIntact) {
  // Consuming a foreign lease would strand its owner's registration forever, which is exactly the bounded state the
  // lease is supposed to guarantee.
  ResponseStore owner;
  ResponseStore other;
  RecordingAdmission admission;

  auto continuation = owner.BeginResponse("", "model-a");
  ASSERT_EQ(owner.InFlightResponses(), 1u);

  EXPECT_FALSE(other.Commit(
      continuation.lease, ResponseStore::StoredResponse{"resp_1", "model-a", ResponseJson("resp_1"), json::array()},
      &admission));
  EXPECT_FALSE(other.Get("resp_1").has_value());
  EXPECT_TRUE(admission.admitted.empty());
  EXPECT_TRUE(static_cast<bool>(continuation.lease)) << "the foreign store consumed a lease it does not own";
  EXPECT_EQ(owner.InFlightResponses(), 1u);

  // Still usable by its owner, and still released normally.
  EXPECT_TRUE(owner.Commit(
      continuation.lease, ResponseStore::StoredResponse{"resp_1", "model-a", ResponseJson("resp_1"), json::array()},
      nullptr));
  EXPECT_EQ(owner.InFlightResponses(), 0u);
}

// ========================================================================
// Delete versus in-flight continuation
// ========================================================================

TEST(ResponseDeleteRaceTest, CommitAdmitsTheSessionExactlyOnceWhenNothingWasDeleted) {
  ResponseStore store;
  RecordingAdmission admission;

  EXPECT_TRUE(RunTurn(store, "resp_1", "", "model-a", &admission));

  EXPECT_EQ(admission.admitted, std::vector<std::string>{"resp_1"});
  EXPECT_TRUE(store.Get("resp_1").has_value());
}

TEST(ResponseDeleteRaceTest, AdmissionFailureLeavesResponseUnpublishedAndPropagates) {
  ResponseStore store;
  ThrowingAdmission admission;
  auto continuation = store.BeginResponse("", "model-a");

  EXPECT_THROW(
      store.Commit(
          continuation.lease,
          ResponseStore::StoredResponse{"resp_1", "model-a", ResponseJson("resp_1"), json::array()},
          &admission),
      std::runtime_error);

  EXPECT_FALSE(store.Get("resp_1").has_value());
  EXPECT_EQ(store.Size(), 0u);
  EXPECT_EQ(store.InFlightResponses(), 0u);
  EXPECT_FALSE(static_cast<bool>(continuation.lease));
  EXPECT_FALSE(admission.rollback_called)
      << "a throwing Admit must clean up its own partial work; rollback is only for later metadata failure";
}

TEST(ResponseDeleteRaceTest, DeletingTheParentMidFlightRejectsTheChildAndCachesNothing) {
  ResponseStore store;
  RecordingAdmission admission;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");

  auto continuation = store.BeginResponse("resp_1", "model-a");
  ASSERT_EQ(continuation.status, ContinuationStatus::kOk);

  // The DELETE lands while resp_2 is still generating.
  ASSERT_TRUE(store.Delete("resp_1"));

  const bool committed = store.Commit(
      continuation.lease,
      ResponseStore::StoredResponse{"resp_2", "model-a", ResponseJson("resp_2", "resp_1"), json::array()},
      &admission);

  EXPECT_FALSE(committed);
  EXPECT_FALSE(store.Get("resp_2").has_value());
  EXPECT_TRUE(admission.admitted.empty()) << "a session was cached under a response derived from deleted content";
  EXPECT_EQ(store.InFlightResponses(), 0u);
}

TEST(ResponseDeleteRaceTest, DeletingAGrandparentMidFlightAlsoRejectsTheChild) {
  ResponseStore store;
  RecordingAdmission admission;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");
  store.Store("resp_2", ResponseJson("resp_2", "resp_1"), json::array(), "model-a");

  auto continuation = store.BeginResponse("resp_2", "model-a");
  ASSERT_EQ(continuation.status, ContinuationStatus::kOk);

  ASSERT_TRUE(store.Delete("resp_1"));

  EXPECT_FALSE(store.Commit(
      continuation.lease,
      ResponseStore::StoredResponse{"resp_3", "model-a", ResponseJson("resp_3", "resp_2"), json::array()},
      &admission));
  EXPECT_TRUE(admission.admitted.empty());
}

TEST(ResponseDeleteRaceTest, DeletingAnUnrelatedConversationLeavesTheInFlightRequestAlone) {
  ResponseStore store;
  RecordingAdmission admission;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");
  store.Store("other_1", ResponseJson("other_1"), json::array(), "model-a");

  auto continuation = store.BeginResponse("resp_1", "model-a");
  ASSERT_EQ(continuation.status, ContinuationStatus::kOk);

  ASSERT_TRUE(store.Delete("other_1"));

  EXPECT_TRUE(store.Commit(
      continuation.lease,
      ResponseStore::StoredResponse{"resp_2", "model-a", ResponseJson("resp_2", "resp_1"), json::array()},
      &admission));
  EXPECT_EQ(admission.admitted, std::vector<std::string>{"resp_2"});
}

TEST(ResponseDeleteRaceTest, DeletingASiblingBranchLeavesTheInFlightRequestAlone) {
  ResponseStore store;
  RecordingAdmission admission;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");
  store.Store("branch_a", ResponseJson("branch_a", "resp_1"), json::array(), "model-a");
  store.Store("branch_b", ResponseJson("branch_b", "resp_1"), json::array(), "model-a");

  auto continuation = store.BeginResponse("branch_a", "model-a");
  ASSERT_EQ(continuation.status, ContinuationStatus::kOk);

  // Deleting the other branch touches neither the shared root nor branch_a.
  ASSERT_TRUE(store.Delete("branch_b"));

  EXPECT_TRUE(store.Commit(
      continuation.lease,
      ResponseStore::StoredResponse{"resp_2", "model-a", ResponseJson("resp_2", "branch_a"), json::array()},
      &admission));
  EXPECT_EQ(admission.admitted, std::vector<std::string>{"resp_2"});
}

TEST(ResponseDeleteRaceTest, DeletingAnAncestorThatWasEvictedMidFlightStillRejectsTheChild) {
  // The reason a lease snapshots its conversation instead of re-walking it at delete time: the parent can be
  // compacted away while the request runs, and its id would then match no stored entry.
  ResponseStore store(2);
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");
  store.Store("resp_2", ResponseJson("resp_2", "resp_1"), json::array(), "model-a");

  auto continuation = store.BeginResponse("resp_2", "model-a");
  ASSERT_EQ(continuation.status, ContinuationStatus::kOk);

  // A third response pushes the conversation's root out of the store; it survives only as a compacted prefix.
  store.Store("resp_x", ResponseJson("resp_x"), json::array(), "model-a");
  ASSERT_FALSE(store.Get("resp_1").has_value());

  ASSERT_TRUE(store.Delete("resp_1")) << "a compacted ancestor must still be deletable";

  RecordingAdmission admission;
  EXPECT_FALSE(store.Commit(
      continuation.lease,
      ResponseStore::StoredResponse{"resp_3", "model-a", ResponseJson("resp_3", "resp_2"), json::array()},
      &admission));
  EXPECT_TRUE(admission.admitted.empty());
}

TEST(ResponseDeleteRaceTest, OnlyTheDependentOfTwoConcurrentRequestsIsRejected) {
  ResponseStore store;
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");
  store.Store("other_1", ResponseJson("other_1"), json::array(), "model-a");

  auto dependent = store.BeginResponse("resp_1", "model-a");
  auto independent = store.BeginResponse("other_1", "model-a");
  ASSERT_EQ(store.InFlightResponses(), 2u);

  ASSERT_TRUE(store.Delete("resp_1"));

  EXPECT_FALSE(store.Commit(
      dependent.lease,
      ResponseStore::StoredResponse{"resp_2", "model-a", ResponseJson("resp_2", "resp_1"), json::array()}, nullptr));
  EXPECT_TRUE(store.Commit(
      independent.lease,
      ResponseStore::StoredResponse{"other_2", "model-a", ResponseJson("other_2", "other_1"), json::array()},
      nullptr));
  EXPECT_EQ(store.InFlightResponses(), 0u);
}

TEST(ResponseDeleteRaceTest, ADeleteThatArrivesAfterTheCommitDropsTheCachedSession) {
  // The other side of the race: once the commit has published both the metadata and the session, the delete finds
  // the child as a dependent and removes both.
  RecordingCache cache;
  ResponseStore store(ResponseStore::kDefaultCapacity, &cache);
  RecordingAdmission admission;

  ASSERT_TRUE(RunTurn(store, "resp_1", "", "model-a", &admission));
  ASSERT_TRUE(RunTurn(store, "resp_2", "resp_1", "model-a", &admission));

  const auto deleted = store.DeleteWithDependents("resp_1");

  EXPECT_EQ(deleted.size(), 2u);
  EXPECT_EQ(cache.dropped, deleted) << "every deleted response must have its cached session dropped";
  EXPECT_FALSE(store.Get("resp_2").has_value());
}

TEST(ResponseDeleteRaceTest, DeletingAnUnknownResponseDropsNothing) {
  RecordingCache cache;
  ResponseStore store(ResponseStore::kDefaultCapacity, &cache);
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");

  EXPECT_FALSE(store.Delete("resp_missing"));
  EXPECT_TRUE(cache.dropped.empty());
}

TEST(ResponseDeleteRaceTest, DeletingACompactedAncestorDropsTheSessionCachedUnderItsOwnId) {
  RecordingCache cache;
  ResponseStore store(2, &cache);
  store.Store("resp_1", ResponseJson("resp_1"), json::array(), "model-a");
  store.Store("resp_2", ResponseJson("resp_2", "resp_1"), json::array(), "model-a");
  store.Store("resp_x", ResponseJson("resp_x"), json::array(), "model-a");
  ASSERT_FALSE(store.Get("resp_1").has_value());

  const auto deleted = store.DeleteWithDependents("resp_1");

  // resp_2 retains resp_1's content in its compacted prefix, so both ids must be purged from the cache.
  EXPECT_EQ(deleted, (std::vector<std::string>{"resp_2", "resp_1"}));
  EXPECT_EQ(cache.dropped, deleted);
}

namespace {

/// Both halves of the cache contract, recorded under one mutex so a concurrent delete and commit can be replayed
/// afterwards.
class ConcurrentCacheRecorder final : public IResponseAdmission, public IResponseCacheCoordinator {
 public:
  void Admit(const std::string& response_id) override {
    std::lock_guard<std::mutex> lock(mutex_);
    admitted_.push_back(response_id);
  }

  void Drop(const std::string& response_id) override {
    std::lock_guard<std::mutex> lock(mutex_);
    dropped_.push_back(response_id);
  }

  void Rollback(const std::string& response_id) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    admitted_.erase(std::remove(admitted_.begin(), admitted_.end(), response_id), admitted_.end());
  }

  bool WasAdmitted(const std::string& response_id) const { return Contains(admitted_, response_id); }
  bool WasDropped(const std::string& response_id) const { return Contains(dropped_, response_id); }

 private:
  bool Contains(const std::vector<std::string>& ids, const std::string& response_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::find(ids.begin(), ids.end(), response_id) != ids.end();
  }

  mutable std::mutex mutex_;
  std::vector<std::string> admitted_;
  std::vector<std::string> dropped_;
};

}  // namespace

TEST(ResponseDeleteRaceTest, ADeleteRacingACommitNeverLeavesTheChildCachedAfterItsAncestorIsGone) {
  // The invariant the lease exists to guarantee, exercised against a real interleaving: whichever order the two
  // operations land in, a response that survives in the cache must also survive in the store. Admitting outside the
  // store's critical section would let the delete slip between the two and reopen the window.
  for (int attempt = 0; attempt < 200; ++attempt) {
    ConcurrentCacheRecorder recorder;
    ResponseStore store(ResponseStore::kDefaultCapacity, &recorder);
    store.Store("root", ResponseJson("root"), json::array(), "model-a");

    auto continuation = store.BeginResponse("root", "model-a");
    ASSERT_EQ(continuation.status, ContinuationStatus::kOk);

    std::thread deleter([&store] { store.Delete("root"); });

    const bool committed = store.Commit(
        continuation.lease,
        ResponseStore::StoredResponse{"child", "model-a", ResponseJson("child", "root"), json::array()}, &recorder);

    deleter.join();

    if (committed) {
      // The delete ran after the commit, so it had to find and drop the child it created.
      EXPECT_TRUE(recorder.WasAdmitted("child"));
      EXPECT_TRUE(recorder.WasDropped("child")) << "child stayed cached after its ancestor was deleted";
    } else {
      EXPECT_FALSE(recorder.WasAdmitted("child")) << "child was cached even though its ancestor was deleted";
    }

    EXPECT_FALSE(store.Get("child").has_value());
    EXPECT_EQ(store.InFlightResponses(), 0u);
  }
}
