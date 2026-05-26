import sys
import os.path
import site
import json
import datetime
import time
import logging
import csv
from logging.handlers import RotatingFileHandler

# Add site directories
site.addsitedir(os.path.join(os.path.dirname(__file__), '../'))
site.addsitedir(os.path.join(os.path.dirname(__file__), '../OfficialScripts'))
site.addsitedir(os.path.join(os.path.dirname(__file__), '../../../../make'))

import urllib.request, urllib.error, urllib.parse
from FedRex import FedWrapper
from FedRex.Utils import Utils
import config  # type: ignore

from Utils import get_logger  # type: ignore
import TaskPool  # type: ignore

# Status code constants
class StatusCode:
    SUCCESS = 200
    NOT_FOUND = 404
    USER_ONLINE = 888
    ERROR = 999

CLIENT_IDS = {
    '2208': {
        'mdc': {
            'beta': '2208:58825:0.0.3:ios:appstore',
        },
        'eur': {
            'gold': '2208:58825:0.0.1geur:ios:appstore',
        }
    },
    '2372': {
        'mdc': {
            'beta': '2372:59921:0.9.9:windows:windows8',
        },
        'eur': {
            'gold': '2372:59921:0.0.1geur:windows:windows8',
        }
    },
    '3101': {
        'mdc': {
            'beta': '3101:65970:0.0.1:steam:steam',
        },
        'eur': {
            'gold': '3101:65970:0.0.1geur:steam:steam',
        }
    }
}

DEFAULT_ENV = 'mdc'
OUTPUT_FILE_CHECK = os.path.join(os.path.dirname(__file__), 'sr12_skin_check_result.csv')
OUTPUT_FILE_ERRORS = os.path.join(os.path.dirname(__file__), 'sr12_skin_errors.csv')
LOG_FILE = os.path.join(os.path.dirname(__file__), 'sr12_skin_log.txt')
FIX_LIST_FILE = os.path.join(os.path.dirname(__file__), 'sr12_skin_fix_needed.json')
OUTPUT_FIX_FAILED = os.path.join(os.path.dirname(__file__), 'sr12_skin_fix_failed.csv')

SR12_SKIN_KEY = 'SR12_HC_Skin'
SR12_SKIN_ATTRS_PATH = '_game_save._loadout.attrs.SR12_HC_Skin'
PROFILE_FIELDS = 'inventory,_game_save._loadout.attrs'
SR12_SKIN_TARGET_ATTRS = {
    'Accuracy': 5,
    'Clip': 5,
    'Thermal': 5,
    'Zoom': 5,
}

CONFIG = {
    # Step 1: scan users from CSV, write check CSV + sr12_skin_fix_needed.json
    'run_inventory_check': True,
    # Step 2: apply fixes (runs after check in same run, or alone from fix list file)
    'apply_loadout_fix': False,
    # CSV: credential_uuid,platform_id[,env]
    'users_input_file': os.path.join(os.path.dirname(__file__), 'users.csv'),
}

##################################################################################################


def setup_file_logger(log_file_path):
    file_logger = logging.getLogger('file_logger')
    file_logger.setLevel(logging.INFO)
    file_logger.handlers = []

    timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    log_file_with_timestamp = log_file_path.replace('.txt', f'_{timestamp}.txt')

    file_handler = RotatingFileHandler(
        log_file_with_timestamp,
        maxBytes=10 * 1024 * 1024,
        backupCount=50,
    )
    file_handler.setLevel(logging.INFO)
    formatter = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S',
    )
    file_handler.setFormatter(formatter)
    file_logger.addHandler(file_handler)
    return file_logger


file_logger = setup_file_logger(LOG_FILE)


def log_to_file(message, level='info'):
    if level.lower() == 'info':
        file_logger.info(message)
    elif level.lower() == 'error':
        file_logger.error(message)
    elif level.lower() == 'warning':
        file_logger.warning(message)
    elif level.lower() == 'debug':
        file_logger.debug(message)
    elif level.lower() == 'critical':
        file_logger.critical(message)


logger = get_logger('SR12 HC Skin loadout fix')


def validate_config(cfg):
    required_keys = (
        'run_inventory_check',
        'apply_loadout_fix',
        'users_input_file',
    )
    for key in required_keys:
        if key not in cfg:
            return False, f"Missing required config key: {key}"
    for flag in ('run_inventory_check', 'apply_loadout_fix'):
        if not isinstance(cfg[flag], bool):
            return False, f"Config '{flag}' must be a boolean"
    if not isinstance(cfg['users_input_file'], str) or not cfg['users_input_file']:
        return False, "Config 'users_input_file' must be a non-empty string"
    return True, ''


def safe_load_json(filepath):
    logger.info('Start loading JSON from: %s', filepath)
    log_to_file(f'Start loading JSON from: {filepath}')
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
        logger.info('Successfully loaded JSON from: %s', filepath)
        log_to_file(f'Successfully loaded JSON from: {filepath}')
        return data
    except FileNotFoundError:
        error_msg = f'File not found: {filepath}'
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        return None
    except json.JSONDecodeError as e:
        error_msg = f'Invalid JSON in file: {filepath}. Error: {e}'
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        return None
    except Exception as e:
        error_msg = f'Unexpected error loading JSON from: {filepath}. Error: {e}'
        logger.exception(error_msg)
        log_to_file(error_msg, 'error')
        return None


def safe_load_csv(filepath):
    logger.info('Start loading CSV from: %s', filepath)
    log_to_file(f'Start loading CSV from: {filepath}')
    rows = []
    try:
        with open(filepath, 'r', newline='', encoding='utf-8-sig') as f:
            reader = csv.reader(f)
            for row in reader:
                if not row:
                    continue
                if len(row) < 2:
                    continue
                credential_raw = (row[0] or '').strip()
                platform_id_raw = (row[1] or '').strip()
                env_raw = (row[2] or '').strip() if len(row) > 2 else ''
                if not credential_raw or not platform_id_raw:
                    continue
                # Skip header row like: credential,platform_id[,env]
                if credential_raw.lower() in ('credential', 'user', 'fed_id', 'uuid') and \
                   platform_id_raw.lower() in ('pid', 'platform', 'platform_id'):
                    continue
                rows.append((credential_raw, platform_id_raw, env_raw))
        logger.info('Successfully loaded CSV rows: %s', len(rows))
        log_to_file(f'Successfully loaded CSV rows: {len(rows)}')
        return rows
    except FileNotFoundError:
        error_msg = f'CSV file not found: {filepath}'
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        return None
    except Exception as e:
        error_msg = f'Unexpected error loading CSV from: {filepath}. Error: {e}'
        logger.exception(error_msg)
        log_to_file(error_msg, 'error')
        return None


def chose_client_id(pid, dc):
    client_id = CLIENT_IDS.get(pid, {}).get(dc, {}).get('beta') or \
        CLIENT_IDS.get(pid, {}).get(dc, {}).get('gold') or ''
    if not client_id:
        error_msg = f'No client ID found for pid: {pid}, dc: {dc}'
        logger.warning(error_msg)
        log_to_file(error_msg, 'warning')
    return client_id


def get_nested_value(profile, field_path):
    val = profile
    for part in field_path.split('.'):
        if isinstance(val, dict):
            val = val.get(part)
        else:
            return None
    return val


def inventory_has_sr12_skin(profile):
    inventory = profile.get('inventory') or {}
    count = inventory.get(SR12_SKIN_KEY)
    if count is None:
        return False, None
    try:
        count_int = int(count)
    except (TypeError, ValueError):
        return False, count
    return count_int >= 1, count_int


def get_loadout_sr12_attrs(profile):
    attrs = get_nested_value(profile, '_game_save._loadout.attrs')
    if not isinstance(attrs, dict):
        return None
    return attrs.get(SR12_SKIN_KEY)


def attrs_need_fix(current_attrs):
    if current_attrs is None:
        return True
    return json.dumps(current_attrs, sort_keys=True) != json.dumps(
        SR12_SKIN_TARGET_ATTRS, sort_keys=True
    )


def evaluate_sr12_skin(profile):
    has_skin, inventory_count = inventory_has_sr12_skin(profile)
    current_attrs = get_loadout_sr12_attrs(profile) if has_skin else None
    should_fix = has_skin and attrs_need_fix(current_attrs)
    return {
        'has_inventory_skin': has_skin,
        'inventory_count': inventory_count,
        'current_attrs': current_attrs,
        'should_fix': should_fix,
    }


def is_user_online(fed_user, credential):
    social_profile_str = fed_user.osiris.get_profile(credential)
    social_profile = json.loads(social_profile_str)
    return social_profile.get('online', False)


def fetch_profile_subset(fed_user, credential):
    profile_str = fed_user.seshat.get_profile(
        selector='',
        credential=credential,
        include_fields=PROFILE_FIELDS,
    )
    return json.loads(profile_str)


def check_sr12_skin_user(credential, fed_user):
    log_to_file(f'[PID: {os.getpid()}] Checking SR12 skin for: {credential}')
    result = {'credential': credential}
    status = StatusCode.SUCCESS
    reason = 'OK'

    try:
        if is_user_online(fed_user, credential):
            result['status'] = StatusCode.USER_ONLINE
            result['reason'] = 'User is online'
            log_to_file(f'User online, skipping: {credential}', 'warning')
            return json.dumps(result)

        profile = fetch_profile_subset(fed_user, credential)
        evaluation = evaluate_sr12_skin(profile)
        result.update(evaluation)

    except urllib.error.HTTPError as e:
        status = e.code
        reason = str(e.read())
        log_to_file(f'HTTPError for {credential}: {status} - {reason}', 'error')
        result['status'] = status
        result['reason'] = reason
        return json.dumps(result)
    except Exception as e:
        reason = Utils.exception_to_string(e)
        log_to_file(f'Exception checking {credential}: {reason}', 'error')
        result['status'] = StatusCode.ERROR
        result['reason'] = reason
        raise

    result['status'] = status
    result['reason'] = reason
    log_to_file(f'Check done for {credential}: should_fix={result.get("should_fix")}')
    return json.dumps(result)


def process_fix_sr12_skin_user(credential, fed_user):
    log_to_file(f'[PID: {os.getpid()}] Fixing SR12 skin attrs for: {credential}')
    result = {'credential': credential}

    try:
        if is_user_online(fed_user, credential):
            result['status'] = StatusCode.USER_ONLINE
            result['reason'] = 'User is online'
            return json.dumps(result)

        profile = fetch_profile_subset(fed_user, credential)
        evaluation = evaluate_sr12_skin(profile)

        if not evaluation['has_inventory_skin']:
            result['status'] = StatusCode.SUCCESS
            result['reason'] = 'No SR12_HC_Skin in inventory (skipped)'
            return json.dumps(result)

        if not evaluation['should_fix']:
            result['status'] = StatusCode.SUCCESS
            result['reason'] = 'Loadout attrs already correct (skipped)'
            return json.dumps(result)

        profile_obj = {SR12_SKIN_ATTRS_PATH: SR12_SKIN_TARGET_ATTRS}
        fed_user.seshat.set_profile(
            object=json.dumps(profile_obj),
            operation='batch_set',
            selector='',
            credential=credential,
        )
        result['status'] = StatusCode.SUCCESS
        result['reason'] = 'Updated _game_save._loadout.attrs.SR12_HC_Skin'
        log_to_file(f'Fixed {credential}: set {SR12_SKIN_ATTRS_PATH}')

    except urllib.error.HTTPError as e:
        result['status'] = e.code
        result['reason'] = str(e.read())
        log_to_file(f'HTTPError fixing {credential}: {result["status"]} - {result["reason"]}', 'error')
    except Exception as e:
        result['status'] = StatusCode.ERROR
        result['reason'] = Utils.exception_to_string(e)
        log_to_file(f'Exception fixing {credential}: {result["reason"]}', 'error')
        raise

    return json.dumps(result)


def prepare_fed_user(client_id, dc):
    log_msg = f'Preparing FedWrapper user with client_id: {client_id}, dc: {dc}'
    logger.info(log_msg)
    log_to_file(log_msg)

    fed_user = FedWrapper.FedWrapper()
    scopes = config.SCOPES_LIST + ' storage_backup'

    try:
        fed_user.initialize(
            client_id=client_id,
            datacenter=dc,
            pandora_url='',
            credential=config.ADMIN_USERNAME,
            password=config.ADMIN_PASSWORD,
            for_credential='',
            scopes=scopes,
            access_token='',
            device_info=None,
        )
        log_to_file('Successfully initialized FedWrapper user')
    except Exception as e:
        error_msg = f'Failed to get access token on {client_id} {dc}: {str(e)}'
        logger.exception(error_msg)
        log_to_file(error_msg, 'error')
        raise

    return fed_user


def prepare_all_fed_users():
    fed_users = {}
    for pid, env_map in CLIENT_IDS.items():
        for env in env_map.keys():
            client_id = chose_client_id(pid, env)
            if not client_id:
                continue
            fed_user = prepare_fed_user(client_id=client_id, dc=env)
            fed_users[(pid, env)] = fed_user
            log_to_file(f'Initialized Fed user for pid={pid}, env={env}')
    return fed_users


def normalize_credential(credential):
    credential = str(credential).strip()
    if credential.startswith('fed_id:'):
        return credential
    return f'fed_id:{credential}'


def resolve_env_from_csv(env_raw):
    env = (env_raw or '').strip().lower()
    if not env:
        return DEFAULT_ENV
    if env not in ('mdc', 'eur'):
        log_to_file(f'Unsupported env "{env_raw}", using default {DEFAULT_ENV}', 'warning')
        return DEFAULT_ENV
    return env


def load_user_targets_from_csv(input_path):
    csv_rows = safe_load_csv(input_path)
    if csv_rows is None:
        return None

    targets = []
    for credential_raw, platform_id_raw, env_raw in csv_rows:
        platform_id = str(platform_id_raw)
        if platform_id not in CLIENT_IDS:
            log_to_file(f'Skipping unsupported platform_id in CSV: {platform_id}', 'warning')
            continue
        targets.append({
            'credential': normalize_credential(credential_raw),
            'platform_id': platform_id,
            'env': resolve_env_from_csv(env_raw),
        })
    return targets


def process_users_with_taskpool(targets, all_fed_users):
    if targets is None:
        error_msg = 'No valid input users found.'
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        return

    total_users = len(targets)
    start_time = time.time()
    logger.info('TaskPool processing START TIME: %s', time.strftime('%Y-%m-%d %H:%M:%S'))
    log_to_file(f'Starting check for {total_users} users')

    jobs = []
    for item in targets:
        cred = item['credential']
        platform_id = item['platform_id']
        env = item['env']
        fed_user = all_fed_users.get((platform_id, env))
        if fed_user is None:
            log_to_file(
                f'Skipping {cred}: Fed user not initialized for platform_id={platform_id}, env={env}',
                'warning',
            )
            continue
        jobs += TaskPool.add_task(
            check_sr12_skin_user,
            (cred, fed_user),
            [],
            get_result=True,
        )

    results = TaskPool.wait_and_get_result(jobs)

    fix_needed = []
    successful = 0
    failed = 0

    with open(OUTPUT_FILE_CHECK, 'w', newline='', encoding='utf-8') as fd_res, \
         open(OUTPUT_FILE_ERRORS, 'w', newline='', encoding='utf-8') as fd_err:
        check_writer = csv.writer(fd_res)
        error_writer = csv.writer(fd_err)
        check_writer.writerow([
            'credential', 'has_inventory_skin', 'inventory_count',
            'current_attrs', 'should_fix',
        ])
        error_writer.writerow(['credential', 'status', 'reason'])

        for r in results:
            try:
                res = json.loads(r)
                cred = res.get('credential', 'unknown')
                if res.get('status') == StatusCode.SUCCESS:
                    successful += 1
                    check_writer.writerow([
                        cred,
                        res.get('has_inventory_skin'),
                        res.get('inventory_count'),
                        json.dumps(res.get('current_attrs')) if res.get('current_attrs') is not None else '',
                        res.get('should_fix'),
                    ])
                    if res.get('should_fix'):
                        matched = next((t for t in targets if t['credential'] == cred), None)
                        if matched:
                            fix_needed.append(matched)
                else:
                    failed += 1
                    error_writer.writerow([
                        cred,
                        res.get('status', 'unknown'),
                        res.get('reason', 'unknown'),
                    ])
            except json.JSONDecodeError as e:
                failed += 1
                error_writer.writerow(['unknown', StatusCode.ERROR, str(e)])
            except Exception as e:
                failed += 1
                error_writer.writerow(['unknown', StatusCode.ERROR, str(e)])

    if fix_needed:
        with open(FIX_LIST_FILE, 'w') as fd_fix:
            json.dump(fix_needed, fd_fix, indent=2)
        log_to_file(f'Wrote {len(fix_needed)} user(s) needing fix to {FIX_LIST_FILE}')

    elapsed = time.time() - start_time
    log_to_file(f'Check complete. Successful: {successful}, Failed: {failed}, Need fix: {len(fix_needed)}')
    print('\n=== CHECK COMPLETE ===')
    print(f'Users checked: {total_users}')
    print(f'Successful: {successful}, Failed: {failed}, Need fix: {len(fix_needed)}')
    print(f'Check results: {OUTPUT_FILE_CHECK}')
    print(f'Fix list: {FIX_LIST_FILE}')
    print(f'Time: {elapsed:.2f}s\n')
    return fix_needed


def run_fix_from_targets(fix_targets, all_fed_users):
    fix_jobs = []
    for item in fix_targets:
        cred = item['credential']
        platform_id = item['platform_id']
        env = item['env']
        fed_user = all_fed_users.get((platform_id, env))
        if fed_user is None:
            log_to_file(
                f'Skipping fix for {cred}: Fed user not initialized for platform_id={platform_id}, env={env}',
                'warning',
            )
            continue
        fix_jobs += TaskPool.add_task(
            process_fix_sr12_skin_user,
            (cred, fed_user),
            [],
            get_result=True,
        )
    return TaskPool.wait_and_get_result(fix_jobs)


def apply_loadout_fixes(fix_targets, all_fed_users):
    if not fix_targets:
        print('No users need fixing.')
        return

    start_time = time.time()
    log_to_file(f'Applying loadout fix for {len(fix_targets)} user(s)')
    fix_results = run_fix_from_targets(fix_targets, all_fed_users)

    fix_ok = 0
    fix_fail = 0
    with open(OUTPUT_FIX_FAILED, 'w', newline='', encoding='utf-8') as fd_fail:
        fail_writer = csv.writer(fd_fail)
        fail_writer.writerow(['credential', 'status', 'reason'])
        for r in fix_results:
            try:
                res = json.loads(r)
                if res.get('status') == StatusCode.SUCCESS:
                    fix_ok += 1
                else:
                    fix_fail += 1
                    fail_writer.writerow([
                        res.get('credential', 'unknown'),
                        res.get('status'),
                        res.get('reason'),
                    ])
            except Exception as e:
                fix_fail += 1
                fail_writer.writerow(['unknown', StatusCode.ERROR, str(e)])

    print('\n=== FIX COMPLETE ===')
    print(f'Fixed: {fix_ok}, Failed: {fix_fail}')
    print(f'Failed details: {OUTPUT_FIX_FAILED}')
    print(f'Time: {time.time() - start_time:.2f}s\n')


def load_fix_targets_from_file(filepath):
    data = safe_load_json(filepath)
    if not data:
        return []

    if isinstance(data, list) and data and isinstance(data[0], dict):
        return data

    # Backward compatibility: list of credential strings
    if isinstance(data, list):
        return [{'credential': normalize_credential(c), 'platform_id': None, 'env': DEFAULT_ENV} for c in data]

    return []


##################################################################################################

if __name__ == '__main__':
    logger.info('Script started')
    log_to_file('Script started')

    is_valid, error_msg = validate_config(CONFIG)
    if not is_valid:
        print(f'ERROR: Configuration validation failed: {error_msg}')
        sys.exit(1)

    run_inventory_check = CONFIG['run_inventory_check']
    apply_loadout_fix = CONFIG['apply_loadout_fix']
    users_input_file = CONFIG['users_input_file']

    targets = load_user_targets_from_csv(users_input_file)
    if not targets:
        log_to_file(f'No valid users loaded from CSV: {users_input_file}', 'error')
        sys.exit(1)

    try:
        all_fed_users = prepare_all_fed_users()
        log_to_file(f'Initialized {len(all_fed_users)} Fed user(s) for all platform/env combinations')
    except Exception as e:
        log_to_file(f'Failed to initialize Fed users: {e}', 'error')
        sys.exit(1)

    fix_needed = []
    if run_inventory_check:
        try:
            fix_needed = process_users_with_taskpool(
                targets=targets,
                all_fed_users=all_fed_users,
            )
        except Exception as e:
            logger.exception('Error during check: %s', e)
            log_to_file(f'Error during check: {e}', 'error')
            sys.exit(1)

    if apply_loadout_fix:
        # Use users from this run's check, or from fix list file (e.g. a later run)
        fix_targets = fix_needed if fix_needed else load_fix_targets_from_file(FIX_LIST_FILE)
        if not fix_targets:
            log_to_file('No users to fix', 'warning')
        else:
            apply_loadout_fixes(fix_targets, all_fed_users)

    logger.info('Script completed')
    log_to_file('Script completed')
