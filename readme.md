# SR12 HC Skin loadout fix (`change_user_profile.py`)

Batch tool that checks player profiles for **SR12_HC_Skin** in inventory and ensures loadout attributes are set correctly in `_game_save._loadout.attrs`.

## What it does

For each user in the input CSV:

1. **Check inventory** — `inventory.SR12_HC_Skin` must be `>= 1`.
2. If the skin is owned, **check loadout attrs** — `_game_save._loadout.attrs.SR12_HC_Skin`.
3. If the key is missing or the value is wrong, mark the user for fix and (when enabled) set:

```json
{
  "Accuracy": 5,
  "Clip": 5,
  "Thermal": 5,
  "Zoom": 5
}
```

Users who are **online** are skipped (not modified).

## Requirements

- Python environment with project dependencies (`FedRex`, `config`, `Utils`, `TaskPool`, etc.).
- Valid admin credentials in `config` (`ADMIN_USERNAME`, `ADMIN_PASSWORD`, `SCOPES_LIST`).
- Input file: CSV list of users (see below).

## Configuration

Edit the `CONFIG` block at the top of `change_user_profile.py`:

| Key | Description |
|-----|-------------|
| `run_inventory_check` | Run the scan and write check results + fix list. |
| `apply_loadout_fix` | Apply profile updates (after check in the same run, or from fix list alone). |
| `users_input_file` | Path to the input CSV. |

Example:

```python
CONFIG = {
    'run_inventory_check': True,
    'apply_loadout_fix': False,
    'users_input_file': os.path.join(os.path.dirname(__file__), 'users.csv'),
}
```

There are **no CLI arguments** — change `CONFIG` and run the script.

## Input CSV format

**Two columns** (environment defaults to `mdc`):

```csv
003dad7e-a91f-11e5-8eda-b8ca3a7093b8,2372
00a1cc65-ff92-11e6-a2d2-b8ca3a60b598,2208
```

**Three columns** (optional environment):

```csv
003dad7e-a91f-11e5-8eda-b8ca3a7093b8,2372,mdc
00a1cc65-ff92-11e6-a2d2-b8ca3a60b598,2208,eur
```

| Column | Description |
|--------|-------------|
| 1 | User UUID (script adds `fed_id:` prefix if missing) |
| 2 | Platform ID: `2208`, `2372`, or `3101` |
| 3 | Optional env: `mdc` or `eur` (default: `mdc`) |

A header row is optional and will be skipped if it looks like `credential,platform_id`.

You do **not** need to convert CSV to JSON.

## Supported platforms and environments

The script initializes Fed connections for all combinations:

| Platform ID | Environment |
|-------------|-------------|
| `2208` | `mdc`, `eur` |
| `2372` | `mdc`, `eur` |
| `3101` | `mdc`, `eur` |

Each CSV row is routed to the correct Fed user using `platform_id` and `env`.

## How to run

```bash
python change_user_profile.py
```

### Recommended workflows

**1. Check only (preview)**

```python
'run_inventory_check': True,
'apply_loadout_fix': False,
```

Review `sr12_skin_check_result.csv` and `sr12_skin_fix_needed.json`, then run again to apply fixes.

**2. Fix later (second run)**

```python
'run_inventory_check': False,
'apply_loadout_fix': True,
```

Reads users from `sr12_skin_fix_needed.json` (written by the check step).

**3. Check and fix in one run**

```python
'run_inventory_check': True,
'apply_loadout_fix': True,
```

Runs check first, then applies fixes to users flagged in that run.

## Execution flow

```
Load users.csv
    → Initialize 6 Fed users (3 platforms × 2 envs)
    → [if run_inventory_check] Scan all users in parallel (TaskPool)
    → Write check CSV + fix list JSON
    → [if apply_loadout_fix] Fix users (from this run or fix list file)
```

Check and fix are **separate steps** in `main` (not `if` / `elif`), so you can check first and fix later in the same run or a later run.

## Output files

| File | Description |
|------|-------------|
| `sr12_skin_check_result.csv` | Per-user check results |
| `sr12_skin_errors.csv` | Users that failed during check (HTTP errors, online, etc.) |
| `sr12_skin_fix_needed.json` | Users to fix (`credential`, `platform_id`, `env`) |
| `sr12_skin_fix_failed.csv` | Users that failed during fix |
| `sr12_skin_log_YYYYMMDD_HHMMSS.txt` | Detailed run log |

### Check result columns

- `credential`
- `has_inventory_skin`
- `inventory_count`
- `current_attrs`
- `should_fix`

## Fix list JSON example

```json
[
  {
    "credential": "fed_id:00a1cc65-ff92-11e6-a2d2-b8ca3a60b598",
    "platform_id": "2208",
    "env": "mdc"
  }
]
```

## Notes

- **Online users** are skipped with status `888` and are not updated.
- Only users with `inventory.SR12_HC_Skin >= 1` are considered for loadout fixes.
- If loadout attrs already match the target object, the user is not fixed again.
- Profile updates use Seshat `batch_set` on `_game_save._loadout.attrs.SR12_HC_Skin`.
