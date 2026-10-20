Numbered SQL migrations `<number>_<name>.sql`, applied by
`automation/fixtures.py` to the working copy when `PRAGMA user_version`
is lower than `<number>`. Folder starts empty: the baseline schema is
born in `Db::init`.
