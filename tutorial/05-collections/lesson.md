# 5. Arrays and Dictionaries

JTML has array literals `[...]` and dictionary literals `{ "key": value }`. Dictionary values are accessed with `dict["key"]`. Assigning to `dict["key"]` mutates reactively — any interpolation or `get` that reads that key updates.

In this lesson, `user` is a dict and `draftName` is the input field's bound state. Pressing **Apply name** copies `draftName` into `user["name"]`, and the page re-renders automatically because the interpolations depend on the dict.
