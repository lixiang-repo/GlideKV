/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/dataset_stateful_op_allowlist.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_def_builder.h"
#include "tensorflow/core/framework/shape_inference.h"

namespace tensorflow {

using shape_inference::DimensionHandle;
using shape_inference::InferenceContext;
using shape_inference::ShapeAndType;
using shape_inference::ShapeHandle;

// --------------------------------------------------------------------------
Status GlideKVValidateTableType(InferenceContext* c,
                         const ShapeAndType& key_shape_and_type,
                         const string& key_dtype_attr,
                         const ShapeAndType& value_shape_and_type,
                         const string& value_dtype_attr) {
  DataType key_dtype;
  TF_RETURN_IF_ERROR(c->GetAttr(key_dtype_attr, &key_dtype));
  if (key_shape_and_type.dtype != key_dtype) {
    return errors::InvalidArgument(
        "Trying to read value with wrong dtype. "
        "Expected ",
        DataTypeString(key_shape_and_type.dtype), " got ",
        DataTypeString(key_dtype));
  }
  DataType value_dtype;
  TF_RETURN_IF_ERROR(c->GetAttr(value_dtype_attr, &value_dtype));
  if (value_shape_and_type.dtype != value_dtype) {
    return errors::InvalidArgument(
        "Trying to read value with wrong dtype. "
        "Expected ",
        DataTypeString(value_shape_and_type.dtype), " got ",
        DataTypeString(value_dtype));
  }
  return OkStatus();
}

Status GlideKVValidateTableResourceHandle(InferenceContext* c, ShapeHandle keys,
                                   const string& key_dtype_attr,
                                   const string& value_dtype_attr,
                                   ShapeAndType* output_shape_and_type) {
  auto* handle_data = c->input_handle_shapes_and_types(0);
  if (handle_data == nullptr || handle_data->size() != 2) {
    output_shape_and_type->shape = c->UnknownShape();
    output_shape_and_type->dtype = DT_INVALID;
  } else {
    const ShapeAndType& key_shape_and_type = (*handle_data)[0];
    const ShapeAndType& value_shape_and_type = (*handle_data)[1];
    TF_RETURN_IF_ERROR(GlideKVValidateTableType(c, key_shape_and_type, key_dtype_attr,
                                         value_shape_and_type,
                                         value_dtype_attr));
    output_shape_and_type->dtype = value_shape_and_type.dtype;
    if (c->RankKnown(key_shape_and_type.shape) && c->RankKnown(keys)) {
      int keys_rank = c->Rank(keys);
      int key_suffix_rank = c->Rank(key_shape_and_type.shape);
      if (keys_rank < key_suffix_rank) {
        return errors::InvalidArgument(
            "Expected keys to have suffix ",
            c->DebugString(key_shape_and_type.shape),
            " but saw shape: ", c->DebugString(keys));
      }
      for (int d = 0; d < key_suffix_rank; d++) {
        // Ensure the suffix of keys match what's in the Table.
        DimensionHandle dim = c->Dim(key_shape_and_type.shape, d);
        TF_RETURN_IF_ERROR(
            c->ReplaceDim(keys, keys_rank - key_suffix_rank + d, dim, &keys));
      }
      std::vector<DimensionHandle> keys_prefix_vec;
      keys_prefix_vec.reserve(keys_rank - key_suffix_rank);
      for (int d = 0; d < keys_rank - key_suffix_rank; ++d) {
        keys_prefix_vec.push_back(c->Dim(keys, d));
      }
      ShapeHandle keys_prefix = c->MakeShape(keys_prefix_vec);
      TF_RETURN_IF_ERROR(c->Concatenate(keys_prefix, value_shape_and_type.shape,
                                        &output_shape_and_type->shape));
    } else {
      output_shape_and_type->shape = c->UnknownShape();
    }
  }
  return OkStatus();
}

Status GlideKVHashTableShape(InferenceContext* c, const ShapeHandle& key,
                             const ShapeHandle& value) {
  c->set_output(0, c->Scalar());

  ShapeHandle key_s;
  TF_RETURN_IF_ERROR(c->WithRankAtMost(key, 1, &key_s));

  DataType key_t;
  TF_RETURN_IF_ERROR(c->GetAttr("key_dtype", &key_t));

  DataType value_t;
  TF_RETURN_IF_ERROR(c->GetAttr("value_dtype", &value_t));

  // ShapeAndType vector for {key, value}.
  c->set_output_handle_shapes_and_types(
      0, std::vector<ShapeAndType>{{key_s, key_t}, {value, value_t}});

  return OkStatus();
}

Status GlideKVHashTableOfTensorsShapeFn(InferenceContext* c) {
  PartialTensorShape value_p;
  TF_RETURN_IF_ERROR(c->GetAttr("value_shape", &value_p));
  ShapeHandle value_s;
  TF_RETURN_IF_ERROR(c->MakeShapeFromPartialTensorShape(value_p, &value_s));
  return GlideKVHashTableShape(c, /*key=*/c->Scalar(), /*value=*/value_s);
}

REGISTER_OP("LookupFind")
    .Input("table_handle: resource")
    .Input("keys: Tin")
    .Input("default_value: Tout")
    .Output("values: Tout")
    .Attr("Tin: type")
    .Attr("Tout: type")
    .SetShapeFn([](InferenceContext* c) {
      ShapeHandle handle;
      TF_RETURN_IF_ERROR(c->WithRank(c->input(0), 0, &handle));

      ShapeAndType value_shape_and_type;
      TF_RETURN_IF_ERROR(GlideKVValidateTableResourceHandle(
          c,
          /*keys=*/c->input(1),
          /*key_dtype_attr=*/"Tin",
          /*value_dtype_attr=*/"Tout", &value_shape_and_type));
      c->set_output(0, value_shape_and_type.shape);

      return OkStatus();
    });
ALLOW_STATEFUL_OP_FOR_DATASET_FUNCTIONS("LookupFind");
// TODO(b/72710477): Update this.

REGISTER_OP("LookupInsert")
    .Input("table_handle: resource")
    .Input("keys: Tin")
    .Input("values: Tout")
    .Attr("Tin: type")
    .Attr("Tout: type")
    .SetShapeFn([](InferenceContext* c) {
      ShapeHandle handle;
      TF_RETURN_IF_ERROR(c->WithRank(c->input(0), 0, &handle));

      // TODO: Validate keys and values shape.
      return OkStatus();
    });

REGISTER_OP("HashTableOfTensors")
    .Output("table_handle: resource")
    .Attr("container: string = ''")
    .Attr("shared_name: string = ''")
    .Attr("use_node_name_sharing: bool = false")
    .Attr("key_dtype: type")
    .Attr("value_dtype: type")
    .Attr("value_shape: shape = {}")
    .Attr("host: string = 'localhost'")
    .Attr("port: int = 3000")
    .Attr("namespace: string = 'test'")
    .Attr("set_name: string = 'vectors'")
    .Attr("field_name: string = 'vector'")
    .SetIsStateful()
    .SetShapeFn(GlideKVHashTableOfTensorsShapeFn);

}  // namespace tensorflow
