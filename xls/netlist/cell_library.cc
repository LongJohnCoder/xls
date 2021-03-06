// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "xls/netlist/cell_library.h"

#include "google/protobuf/repeated_field.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "xls/common/integral_types.h"
#include "xls/common/status/ret_check.h"
#include "xls/common/status/status_macros.h"
#include "xls/netlist/netlist.pb.h"

namespace xls {
namespace netlist {
namespace {

xabsl::StatusOr<CellKind> CellKindFromProto(CellKindProto proto) {
  switch (proto) {
    case INVALID:
      break;
    case FLOP:
      return CellKind::kFlop;
    case INVERTER:
      return CellKind::kInverter;
    case BUFFER:
      return CellKind::kBuffer;
    case NAND:
      return CellKind::kNand;
    case NOR:
      return CellKind::kNor;
    case MULTIPLEXER:
      return CellKind::kMultiplexer;
    case XOR:
      return CellKind::kXor;
    case OTHER:
      return CellKind::kOther;
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "Invalid proto value for conversion to CellKind: %d", proto));
}

xabsl::StatusOr<StateTableSignal> StateTableSignalFromProto(
    StateTableSignalProto proto) {
  switch (proto) {
    case STATE_TABLE_SIGNAL_INVALID:
      return StateTableSignal::kInvalid;
    case STATE_TABLE_SIGNAL_LOW:
      return StateTableSignal::kLow;
    case STATE_TABLE_SIGNAL_HIGH:
      return StateTableSignal::kHigh;
    case STATE_TABLE_SIGNAL_DONTCARE:
      return StateTableSignal::kDontCare;
    case STATE_TABLE_SIGNAL_HIGH_OR_LOW:
      return StateTableSignal::kHighOrLow;
    case STATE_TABLE_SIGNAL_LOW_OR_HIGH:
      return StateTableSignal::kLowOrHigh;
    case STATE_TABLE_SIGNAL_RISING:
      return StateTableSignal::kRising;
    case STATE_TABLE_SIGNAL_FALLING:
      return StateTableSignal::kFalling;
    case STATE_TABLE_SIGNAL_NOT_RISING:
      return StateTableSignal::kNotRising;
    case STATE_TABLE_SIGNAL_NOT_FALLING:
      return StateTableSignal::kNotFalling;
    case STATE_TABLE_SIGNAL_NOCHANGE:
      return StateTableSignal::kNoChange;
    case STATE_TABLE_SIGNAL_X:
      return StateTableSignal::kX;
    default:
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid proto value for conversion to StateTableSignal: %d", proto));
  }
}

xabsl::StatusOr<StateTableSignalProto> ProtoFromStateTableSignal(
    StateTableSignal signal) {
  switch (signal) {
    case StateTableSignal::kInvalid:
      return STATE_TABLE_SIGNAL_INVALID;
    case StateTableSignal::kLow:
      return STATE_TABLE_SIGNAL_LOW;
    case StateTableSignal::kHigh:
      return STATE_TABLE_SIGNAL_HIGH;
    case StateTableSignal::kDontCare:
      return STATE_TABLE_SIGNAL_DONTCARE;
    case StateTableSignal::kHighOrLow:
      return STATE_TABLE_SIGNAL_HIGH_OR_LOW;
    case StateTableSignal::kLowOrHigh:
      return STATE_TABLE_SIGNAL_LOW_OR_HIGH;
    case StateTableSignal::kRising:
      return STATE_TABLE_SIGNAL_RISING;
    case StateTableSignal::kFalling:
      return STATE_TABLE_SIGNAL_FALLING;
    case StateTableSignal::kNotRising:
      return STATE_TABLE_SIGNAL_NOT_RISING;
    case StateTableSignal::kNotFalling:
      return STATE_TABLE_SIGNAL_NOT_FALLING;
    case StateTableSignal::kNoChange:
      return STATE_TABLE_SIGNAL_NOCHANGE;
    case StateTableSignal::kX:
      return STATE_TABLE_SIGNAL_X;
    default:
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid proto value for conversion to StateTableSignal: %d",
          static_cast<int>(signal)));
  }
}

}  // namespace

std::string CellKindToString(CellKind kind) {
  switch (kind) {
    case CellKind::kFlop:
      return "flop";
    case CellKind::kInverter:
      return "inverter";
    case CellKind::kBuffer:
      return "buffer";
    case CellKind::kNand:
      return "nand";
    case CellKind::kNor:
      return "nor";
    case CellKind::kXor:
      return "xor";
    case CellKind::kMultiplexer:
      return "multiplexer";
    case CellKind::kOther:
      return "other";
  }
  return absl::StrFormat("<invalid CellKind(%d)>", static_cast<int64>(kind));
}

/* static */ xabsl::StatusOr<StateTable> StateTable::FromProto(
    const StateTableProto& proto) {
  std::vector<Row> rows;
  absl::flat_hash_set<std::string> signals;
  for (const auto& row : proto.rows()) {
    RowStimulus stimulus;
    for (const auto& kv : row.input_signals()) {
      XLS_ASSIGN_OR_RETURN(stimulus[kv.first],
                           StateTableSignalFromProto(kv.second));
      signals.insert(kv.first);
    }
    for (const auto& kv : row.internal_signals()) {
      XLS_ASSIGN_OR_RETURN(stimulus[kv.first],
                           StateTableSignalFromProto(kv.second));
      signals.insert(kv.first);
    }

    RowResponse response;
    for (const auto& kv : row.next_internal_signals()) {
      XLS_ASSIGN_OR_RETURN(response[kv.first],
                           StateTableSignalFromProto(kv.second));
    }

    rows.push_back({stimulus, response});
  }

  return StateTable(rows, signals, proto);
}

StateTable::StateTable(const std::vector<Row>& rows,
                       const absl::flat_hash_set<std::string>& signals,
                       const StateTableProto& proto)
    : signals_(signals), rows_(rows), proto_(proto) {
  // Get the "output" side ("RowResponse") of the first row in the table,
  // and cache the signal names from there.
  for (const auto& kv : rows_[0].response) {
    internal_signals_.insert(kv.first);
  }
}

// TODO(rspringer): 2020/08/28 - We don't handle transition signals (rising,
// falling, etc.) here yet.
bool StateTable::SignalMatches(absl::string_view name, bool value,
                               const RowStimulus& stimulus) const {
  // Input signals can be either high or low - we don't manage persistent state
  // inside this class; we just represent the table.
  StateTableSignal value_signal =
      value ? StateTableSignal::kHigh : StateTableSignal::kLow;
  // If the stimulus is L/H or H/L, it has to match; we'll figure out what to do
  // with the output later (in GetSignalValue()).
  if (stimulus.contains(name) &&
      (stimulus.at(name) == StateTableSignal::kLowOrHigh ||
       stimulus.at(name) == StateTableSignal::kHighOrLow ||
       value_signal == stimulus.at(name) ||
       stimulus.at(name) == StateTableSignal::kDontCare)) {
    return true;
  }

  return false;
}

xabsl::StatusOr<bool> StateTable::MatchRow(
    const Row& row, const InputStimulus& input_stimulus) const {
  absl::flat_hash_set<std::string> unspecified_inputs = signals_;
  const RowStimulus& row_stimulus = row.stimulus;
  for (const auto& kv : input_stimulus) {
    const std::string& name = kv.first;
    bool value = kv.second;

    XLS_RET_CHECK(signals_.contains(name));
    // Check that, for every stimulus signal, that there's not a mismatch
    // (i.e., the row has "don't care" or a matching value for the input.
    if (!SignalMatches(name, value, row_stimulus)) {
      return false;
    }

    // We've matched this input!
    unspecified_inputs.erase(name);
  }

  // We've matched all inputs - now we have to verify that all unspecified
  // inputs are "don't care".
  for (const std::string& name : unspecified_inputs) {
    if (row_stimulus.at(name) != StateTableSignal::kDontCare) {
      return false;
    }
  }
  return true;
}

xabsl::StatusOr<bool> StateTable::GetSignalValue(
    const InputStimulus& input_stimulus, absl::string_view signal) const {
  // Find a row matching the stimulus or return error.
  XLS_RET_CHECK(std::find(proto_.internal_names().begin(),
                          proto_.internal_names().end(),
                          signal) != proto_.internal_names().end())
      << "Can't find internal signal \"" << signal << "\"";

  for (const Row& row : rows_) {
    XLS_ASSIGN_OR_RETURN(bool match, MatchRow(row, input_stimulus));
    if (match) {
      // If "switched" (e.g., kLowOrHigh) are used, then the next value is
      // identity if both sides are the same, and inversion otherwise.
      StateTableSignal stimulus_signal = row.stimulus.at(signal);
      StateTableSignal response_signal = row.response.at(signal);
      bool switched_signal = stimulus_signal == StateTableSignal::kLowOrHigh ||
                             stimulus_signal == StateTableSignal::kHighOrLow;
      if (switched_signal) {
        if (stimulus_signal == response_signal) {
          return input_stimulus.at(signal);
        } else {
          return !input_stimulus.at(signal);
        }
      }

      return response_signal == StateTableSignal::kHigh;
    }
  }

  return absl::NotFoundError("No matching row found in the table.");
}

xabsl::StatusOr<StateTableProto> StateTable::ToProto() const {
  StateTableProto proto;
  // First, extract the signals (internal and input) from the first row (each
  // row contains every signal).
  for (const auto& kv_stimulus : rows_[0].stimulus) {
    const std::string& signal_name = kv_stimulus.first;
    if (internal_signals_.contains(signal_name)) {
      proto.add_internal_names(signal_name);
    } else {
      proto.add_input_names(signal_name);
    }
  }

  for (const Row& row : rows_) {
    StateTableRow* row_proto = proto.add_rows();
    for (const auto& kv_stimulus : row.stimulus) {
      const std::string& signal_name = kv_stimulus.first;
      XLS_ASSIGN_OR_RETURN(StateTableSignalProto signal_value,
                           ProtoFromStateTableSignal(kv_stimulus.second));
      if (internal_signals_.contains(signal_name)) {
        row_proto->mutable_internal_signals()->insert(
            {signal_name, signal_value});
      } else {
        row_proto->mutable_input_signals()->insert({signal_name, signal_value});
      }
    }

    for (const auto& kv_response : row.response) {
      XLS_ASSIGN_OR_RETURN(StateTableSignalProto signal_value,
                           ProtoFromStateTableSignal(kv_response.second));
      row_proto->mutable_next_internal_signals()->insert(
          {kv_response.first, signal_value});
    }
  }
  return proto;
}

/* static */ xabsl::StatusOr<CellLibraryEntry> CellLibraryEntry::FromProto(
    const CellLibraryEntryProto& proto) {
  XLS_ASSIGN_OR_RETURN(CellKind cell_kind, CellKindFromProto(proto.kind()));

  OutputPinListProto output_pin_list = proto.output_pin_list();
  OutputPinToFunction pins;
  pins.reserve(output_pin_list.pins_size());
  for (const auto& proto : output_pin_list.pins()) {
    pins[proto.name()] = proto.function();
  }

  absl::optional<StateTable> state_table;
  if (proto.has_state_table()) {
    XLS_ASSIGN_OR_RETURN(state_table,
                         StateTable::FromProto(proto.state_table()));
  }

  return CellLibraryEntry(cell_kind, proto.name(), proto.input_names(), pins,
                          state_table);
}

xabsl::StatusOr<CellLibraryEntryProto> CellLibraryEntry::ToProto() const {
  CellLibraryEntryProto proto;
  switch (kind_) {
    case CellKind::kFlop:
      proto.set_kind(CellKindProto::FLOP);
      break;
    case CellKind::kInverter:
      proto.set_kind(CellKindProto::INVERTER);
      break;
    case CellKind::kBuffer:
      proto.set_kind(CellKindProto::BUFFER);
      break;
    case CellKind::kNand:
      proto.set_kind(CellKindProto::NAND);
      break;
    case CellKind::kNor:
      proto.set_kind(CellKindProto::NOR);
      break;
    case CellKind::kMultiplexer:
      proto.set_kind(CellKindProto::MULTIPLEXER);
      break;
    case CellKind::kXor:
      proto.set_kind(CellKindProto::XOR);
      break;
    case CellKind::kOther:
      proto.set_kind(CellKindProto::OTHER);
      break;
  }
  proto.set_name(name_);
  for (const std::string& input_name : input_names_) {
    proto.add_input_names(input_name);
  }
  if (state_table_.has_value()) {
    XLS_ASSIGN_OR_RETURN(*proto.mutable_state_table(),
                         state_table_.value().ToProto());
  }

  OutputPinListProto* pin_list = proto.mutable_output_pin_list();
  for (const auto& kv : output_pin_to_function_) {
    OutputPinProto* pin_proto = pin_list->add_pins();
    pin_proto->set_name(kv.first);
    pin_proto->set_function(kv.second);
  }
  return proto;
}

/* static */ xabsl::StatusOr<CellLibrary> CellLibrary::FromProto(
    const CellLibraryProto& proto) {
  CellLibrary cell_library;
  for (const CellLibraryEntryProto& entry_proto : proto.entries()) {
    XLS_ASSIGN_OR_RETURN(auto entry, CellLibraryEntry::FromProto(entry_proto));
    XLS_RETURN_IF_ERROR(cell_library.AddEntry(std::move(entry)));
  }
  return cell_library;
}

xabsl::StatusOr<CellLibraryProto> CellLibrary::ToProto() const {
  CellLibraryProto proto;
  for (const auto& entry : entries_) {
    XLS_ASSIGN_OR_RETURN(*proto.add_entries(), entry.second->ToProto());
  }
  return proto;
}

absl::Status CellLibrary::AddEntry(CellLibraryEntry entry) {
  if (entries_.find(entry.name()) != entries_.end()) {
    return absl::FailedPreconditionError(
        "Attempting to register a cell library entry with a duplicate name: " +
        entry.name());
  }
  entries_.insert({entry.name(), absl::make_unique<CellLibraryEntry>(entry)});
  return absl::OkStatus();
}

xabsl::StatusOr<const CellLibraryEntry*> CellLibrary::GetEntry(
    absl::string_view name) const {
  auto it = entries_.find(name);
  if (it == entries_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Cell not found in library: ", name));
  }
  return it->second.get();
}

}  // namespace netlist
}  // namespace xls
