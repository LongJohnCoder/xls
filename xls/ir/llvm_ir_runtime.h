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

#ifndef XLS_IR_LLVM_IR_RUNTIME_H_
#define XLS_IR_LLVM_IR_RUNTIME_H_

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "xls/common/integral_types.h"
#include "xls/ir/llvm_type_converter.h"
#include "xls/ir/type.h"
#include "xls/ir/value.h"

namespace xls {

// LlvmIrRuntime contains routines necessary for executing code generated by the
// IR JIT. For type resolution, the JIT packs input data into and pulls
// data out of a flat character buffer, thus these routines are necessary.
class LlvmIrRuntime {
 public:
  LlvmIrRuntime(const llvm::DataLayout& data_layout,
                LlvmTypeConverter* type_converter);

  // Packs the specified values into a flat buffer with the data layout
  // expected by LLVM.
  // "arg_buffers" must contain an entry corresponding to each element in
  // "args", with a matching amount of space allocated.
  absl::Status PackArgs(absl::Span<const Value> args,
                        absl::Span<Type* const> arg_types,
                        absl::Span<uint8*> arg_buffers);

  // Returns a Value constructed from the data inside "buffer" whose
  // contents are laid out according to the LLVM interpretation of the passed-in
  // type.
  Value UnpackBuffer(const uint8* buffer, const Type* result_type);

  // Splats the value into the buffer according to the data layout expected by
  // LLVM.
  void BlitValueToBuffer(const Value& value, const Type& type,
                         absl::Span<uint8> buffer);

  // Returns a textual description of the argument LLVM object.
  template <typename T>
  static std::string DumpToString(const T& llvm_object);

 private:
  llvm::DataLayout data_layout_;
  LlvmTypeConverter* type_converter_;
};

template <>
std::string LlvmIrRuntime::DumpToString<llvm::Module>(
    const llvm::Module& llvm_object);

extern "C" {

// Below are defined simple C interfaces to the LlvmIrRuntime functions above,
// currently only for use by LLVM IR samples wrapped in "main" functions
// generated by llvm_main_generator.cc. Such test cases are for debugging
// differences between the LLVM IR JIT and the XLS IR interpreter.

// Parses the set of args (as "int argc, char** argv" that contain textual
// representations of XLS IR Values and determines how much storage is needed to
// contain them as LLVM Values format.
// On failure, a negative value will be returned.
int64 GetArgBufferSize(int arg_count, const char** input_args);

// Packs the set of args (as above) into the specified buffer. This buffer must
// be large enough to contain the LLVM Value representation of these values.
// On failure, a negative value will be returned, otherwise this returns 0.
int64 PackArgs(int arg_count, const char** input_args, uint8** buffer);

// Takes in a buffer containing LLVM-packed data and converts into an XLS Value,
// which is then printed to stdout.
int UnpackAndPrintBuffer(const char* output_type_string, int arg_count,
                         const char** input_args, const uint8* buffer);

}  // extern "C"

}  // namespace xls

#endif  // XLS_IR_LLVM_IR_RUNTIME_H_
