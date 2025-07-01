# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Lookup operations."""
# pylint: disable=g-bad-name

from tensorflow.python.eager import context
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
# pylint: enable=wildcard-import
from tensorflow.python.trackable import resource
from tensorflow.python.training.saver import BaseSaverBuilder
import tensorflow as tf
import os

here = os.path.abspath(os.path.dirname(__file__))
_ops = tf.load_op_library(os.path.join(here, "_lookup_ops.so"))


class LookupInterface(resource.TrackableResource):
  """Represent a lookup table that persists across different steps."""

  def __init__(self, key_dtype, value_dtype):
    """Construct a lookup table interface.

    Args:
      key_dtype: The table key type.
      value_dtype: The table value type.
    """
    self._key_dtype = dtypes.as_dtype(key_dtype)
    self._value_dtype = dtypes.as_dtype(value_dtype)
    super(LookupInterface, self).__init__()

  def _create_resource(self):
    raise NotImplementedError

  @property
  def key_dtype(self):
    """The table key dtype."""
    return self._key_dtype

  @property
  def value_dtype(self):
    """The table value dtype."""
    return self._value_dtype

  @property
  def name(self):
    """The name of the table."""
    return NotImplementedError

  def lookup(self, keys, name=None):
    """Looks up `keys` in a table, outputs the corresponding values."""
    raise NotImplementedError

  def __getitem__(self, keys):
    """Looks up `keys` in a table, outputs the corresponding values."""
    return self.lookup(keys)

class LookupTable(LookupInterface):
  """A generic mutable lookup table implementation.

  Data can be inserted by calling the `insert` method and removed by calling the
  `remove` method. It does not support initialization via the init method.

  `LookupTable` requires additional memory during checkpointing and restore
  operations to create temporary key and value tensors.
  """

  def __init__(self,
               dim,
               host,
               port,
               namespace,
               set_name,
               field_name,
               key_dtype=tf.int64,
               value_dtype=tf.float32,
               name="LookupTable"):
    """Creates an empty `LookupTable` object.

    Creates a table, the type of its keys and values are specified by key_dtype
    and value_dtype, respectively.

    Args:
      dim: embedding dimension
      host: host
      port: port
      namespace: namespace
      set_name: set_name
      field_name: field name
      key_dtype: the type of the key tensors.
      value_dtype: the type of the value tensors.
      name: A name for the operation (optional).

    Returns:
      A `LookupTable` object.
    """
    self._dim = dim
    self._host = host
    self._port = port
    self._namespace = namespace
    self._set_name = set_name
    self._field_name = field_name

    self._default_value = ops.convert_to_tensor([0.0] * self._dim, dtype=value_dtype)
    self._key_dtype = key_dtype
    self._value_dtype = value_dtype
    self._name = name
    self._shared_name = None
    if context.executing_eagerly():
        # TODO(allenl): This will leak memory due to kernel caching by
        # the shared_name attribute value (but is better than the
        # alternative of sharing everything by default when executing
        # eagerly; hopefully creating tables in a loop is uncommon).
        self._shared_name = "table_%d" % (ops.uid(),)
    super(LookupTable, self).__init__(key_dtype, value_dtype)
    self._resource_handle = self._create_resource()

  def _create_resource(self):
    # The table must be shared if checkpointing is requested for multi-worker
    # training to work correctly. Use the node name if no shared_name has been
    # explicitly specified.
    table_ref = _ops.hash_table_of_tensors(
        shared_name=self._shared_name,
        key_dtype=self._key_dtype,
        value_dtype=self._value_dtype,
        value_shape=self._default_value.get_shape(),
        host=self._host,
        port=self._port,
        namespace=self._namespace,
        set_name=self._set_name,
        field_name=self._field_name,
        name=self._name)

    if context.executing_eagerly():
      self._table_name = None
    else:
      self._table_name = table_ref.op.name.split("/")[-1]
    return table_ref

  @property
  def name(self):
    return self._table_name

  def lookup(self, keys, name=None):
    """Looks up `keys` in a table, outputs the corresponding values.

    The `_default_value` is used for keys not present in the table.

    Args:
      keys: Keys to look up. Can be a tensor of any shape. Must match the
        table's key_dtype.
      name: A name for the operation (optional).

    Returns:
      A tensor containing the values in the same shape as `keys` using the
        table's value type.

    Raises:
      TypeError: when `keys` do not match the table data types.
    """
    with ops.name_scope(name, "%s_lookup_find" % self.name,
                        (self.resource_handle, keys, self._default_value)):
      keys = ops.convert_to_tensor(keys, dtype=self._key_dtype, name="keys")
      with ops.colocate_with(self.resource_handle):
        values = _ops.lookup_find(self.resource_handle, keys, self._default_value)
    return values


ops.NotDifferentiable("LookupFind")
ops.NotDifferentiable("HashTableOfTensors")
