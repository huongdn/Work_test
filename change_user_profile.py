import sys
import glob
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
import config # type: ignore
from GetSeshatProfile import get_seshat_profile # type: ignore
from SetSeshatProfile import set_seshat_profile # type: ignore

# Import your existing logger function
from Utils import get_logger # type: ignore
import TaskPool # type: ignore

# Status code constants
class StatusCode:
    """Status code constants for API responses."""
    SUCCESS = 200
    NOT_FOUND = 404
    USER_ONLINE = 888
    ERROR = 999

# Field type constants
class FieldType:
    """Field type constants for compensation logic."""
    NORMAL = 'NORMAL'  # Normal numeric fields - use compensation calculation
    RESTORE = 'RESTORE'  # Fields that should be restored to start_date backup value (timestamp, leaderboard_id, etc.)
    MILESTONE = 'MILESTONE'  # Milestone fields - use max value from start, end, and current
    OBJECT = 'OBJECT'  # Nested object fields - restore entire object with all sub-fields from start_date backup

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

INPUT_FILE = os.path.join(os.path.dirname(__file__), 'my_data.json')
OUTPUT_FILE_CHECK = os.path.join(os.path.dirname(__file__), 'my_data_check_result.csv')
OUTPUT_FILE_ERRORS = os.path.join(os.path.dirname(__file__), 'my_data_errors.csv')
LOG_FILE = os.path.join(os.path.dirname(__file__), 'my_data_log.txt')
COMPENSATION_LIST = os.path.join(os.path.dirname(__file__), 'compensation_needed.json')
OUTPUT_COMPENSATE_FAILED = os.path.join(os.path.dirname(__file__), 'my_data_compensate_failed.csv')

# Script configuration (edit these values as needed)
CONFIG = {
    # Set to True to skip compensation execution (checks will still run) default True
    'dry_run': True,
    # Whether to run the main check phase to generate compensation_needed.json default True
    'get_compensation': True,
    # Whether to run test compensation from COMPENSATION_LIST (compensation_needed.json) default False
    'run_test': False,
    # Platform and datacenter
    'pid': "2208",
    'dc': "mdc",
    # Fields to check; supports string (defaults to NORMAL) or dict with 'field' and 'type'
    'fields_to_check': [
        {"field": "_pvp_stats.scratchpad.elo", "type": FieldType.NORMAL},
        {"field": "_mp_stats.score", "type": FieldType.NORMAL},
        {"field": "_mp_stats.lb", "type": FieldType.RESTORE},
        {"field": "_defender_leaderboard.elo", "type": FieldType.NORMAL},
        {"field": "_defender_leaderboard.personal_milestone", "type": FieldType.MILESTONE},
        {"field": "_defender_leaderboard.ts", "type": FieldType.RESTORE},
        {"field": "inventory.clan_battlepass", "type": FieldType.MILESTONE},
        {"field": "inventory.clan_battlepass_xp", "type": FieldType.NORMAL},
        {"field": "_game_save.clanBP.lb", "type": FieldType.RESTORE},
        {"field": "_game_save.clanBP.claimedRewards", "type": FieldType.MILESTONE}, # Need recheck
        {"field": "_game_save.clanBP.claimedRewardsPremium", "type": FieldType.MILESTONE}, # Need recheck
        {"field": "_game_save.clanBP.unlockedChapters", "type": FieldType.MILESTONE}, # Need recheck
        # {"field": "_game_save.clanBP.premiumMembers", "type": FieldType.OBJECT}, # Need recheck
        {"field": "inventory.defender_bas_item_duration_buff", "type": FieldType.MILESTONE},
        {"field": "inventory.defender_gold_cash_buff", "type": FieldType.MILESTONE},
        {"field": "inventory.defender_pvp_pack_buff", "type": FieldType.MILESTONE},
        {"field": "inventory.defender_pvp_pack_cooldown_buff", "type": FieldType.MILESTONE},
        {"field": "inventory.defender_trophy_clan_cash_buff", "type": FieldType.MILESTONE},
        {"field": "inventory.defender_vault_cap_buff", "type": FieldType.MILESTONE},
        {"field": "inventory.defender_vault_gen_buff", "type": FieldType.MILESTONE},
        {"field": "inventory.battlepass_complete_chapter", "type": FieldType.MILESTONE},
        {"field": "inventory.battlepass_platinum", "type": FieldType.MILESTONE},
        {"field": "inventory.battlepass_plus", "type": FieldType.MILESTONE},
        {"field": "inventory.battlepass_rush_chapter", "type": FieldType.MILESTONE},
        {"field": "inventory.battlepass_unlocked", "type": FieldType.MILESTONE},
        {"field": "inventory.conq", "type": FieldType.NORMAL}, # Outpost score
        # {"field": "_game_save._battle_pass", "type": FieldType.OBJECT},
        {"field": "_game_save._battle_pass.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.CCP.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.CGP.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.CNP.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.CPC.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.CRC.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.CSU.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.CTP.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FBS.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FDT.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FPE.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FPR.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FPU.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FRL.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FRM.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FSE.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FSL.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FSX.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.FTS.lb", "type": FieldType.RESTORE},
        # {"field": "_conq_outposts.MDF.lb", "type": FieldType.RESTORE},
        # {"field": "_game_save.clanBP.xpBonusTs", "type": FieldType.MILESTONE}, # Need recheck
        # Example of restore field (timestamp, leaderboard_id, etc.):
        # {"field": "_pvp_stats.scratchpad.last_season_timestamp", "type": FieldType.RESTORE},
        # {"field": "_pvp_stats.scratchpad.leaderboard_id", "type": FieldType.RESTORE},
        # Example of object field (restores entire nested object with all sub-fields):
        # {"field": "_game_save._battle_pass", "type": FieldType.OBJECT},
        # You can also use string format (backward compatible):
        # "_pvp_stats.scratchpad.elo"  # defaults to FieldType.NORMAL
    ],
    # Backup comparison
    'start_date': "2025-12-21",
    'end_date': "2025-12-22"
}

##################################################################################################

def setup_file_logger(log_file_path):
    """Setup a logger that creates a new file for each run."""
    file_logger = logging.getLogger('file_logger')
    file_logger.setLevel(logging.INFO)
    
    # Remove existing handlers
    file_logger.handlers = []
    
    # Create new file with timestamp (recommended)
    timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    log_file_with_timestamp = log_file_path.replace('.txt', f'_{timestamp}.txt')
    
    file_handler = RotatingFileHandler(
        log_file_with_timestamp,
        maxBytes=10*1024*1024,
        backupCount=50
    )
    file_handler.setLevel(logging.INFO)
    
    formatter = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )
    file_handler.setFormatter(formatter)
    file_logger.addHandler(file_handler)
    
    return file_logger

# Setup file logger
file_logger = setup_file_logger(LOG_FILE)

def log_to_file(message, level='info'):
    """Helper function to log messages to the file."""
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

# Use your existing logger for console/logging system
logger = get_logger('Fix season point error')

def validate_config(config):
    """Validate CONFIG dictionary has required fields and valid values.
    
    Args:
        config: Configuration dictionary to validate
        
    Returns:
        tuple: (is_valid: bool, error_message: str)
    """
    required_keys = ['dry_run', 'get_compensation', 'run_test', 'pid', 'dc', 'fields_to_check', 'start_date', 'end_date']
    
    # Check required keys exist
    for key in required_keys:
        if key not in config:
            return False, f"Missing required config key: {key}"
    
    # Validate boolean flags
    if not isinstance(config['dry_run'], bool):
        return False, "Config 'dry_run' must be a boolean"
    if not isinstance(config['get_compensation'], bool):
        return False, "Config 'get_compensation' must be a boolean"
    if not isinstance(config['run_test'], bool):
        return False, "Config 'run_test' must be a boolean"
    
    # Validate pid and dc are strings
    if not isinstance(config['pid'], str) or not config['pid']:
        return False, "Config 'pid' must be a non-empty string"
    
    if not isinstance(config['dc'], str) or not config['dc']:
        return False, "Config 'dc' must be a non-empty string"
    
    # Validate fields_to_check is a list
    if not isinstance(config['fields_to_check'], list) or len(config['fields_to_check']) == 0:
        return False, "Config 'fields_to_check' must be a non-empty list"
    
    # Validate date format (YYYY-MM-DD)
    date_format = "%Y-%m-%d"
    for date_key in ['start_date', 'end_date']:
        date_str = config[date_key]
        if not isinstance(date_str, str):
            return False, f"Config '{date_key}' must be a string"
        try:
            datetime.datetime.strptime(date_str, date_format)
        except ValueError:
            return False, f"Config '{date_key}' must be in format YYYY-MM-DD, got: {date_str}"
    
    # Validate start_date < end_date
    start_dt = datetime.datetime.strptime(config['start_date'], date_format)
    end_dt = datetime.datetime.strptime(config['end_date'], date_format)
    if start_dt >= end_dt:
        return False, f"Config 'start_date' ({config['start_date']}) must be before 'end_date' ({config['end_date']})"
    
    return True, ""

def safe_load_json(filepath):
    """
    Loads a JSON file safely, handling potential errors.
    """
    logger.info("Start loading JSON from: %s", filepath)
    log_to_file(f"Start loading JSON from: {filepath}")
    
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
        logger.info("Successfully loaded JSON from: %s", filepath)
        log_to_file(f"Successfully loaded JSON from: {filepath}")
        return data
    except FileNotFoundError:
        error_msg = f"File not found: {filepath}"
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        return None
    except json.JSONDecodeError as e:
        error_msg = f"Invalid JSON in file: {filepath}. Error: {e}"
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        return None
    except Exception as e:
        error_msg = f"An unexpected error occurred while loading JSON from: {filepath}. Error: {e}"
        logger.exception(error_msg)
        log_to_file(error_msg, 'error')
        return None

def chose_client_id(pid, dc):
    """Get client ID for given platform and datacenter."""
    client_id = CLIENT_IDS.get(pid, {}).get(dc, {}).get('beta') or \
                CLIENT_IDS.get(pid, {}).get(dc, {}).get('gold') or ""
    
    if not client_id:
        error_msg = f"No client ID found for pid: {pid}, dc: {dc}"
        logger.warning(error_msg)
        log_to_file(error_msg, 'warning')
    
    return client_id

def process_compensate_user(credential, fields, fed_user):
    log_msg = f"[PID: {os.getpid()}] Starting process_compensate_user for credential: {credential}"
    log_to_file(log_msg)
    result = {'credential': credential}
    status = StatusCode.SUCCESS
    reason = 'OK'

    # First check if user is online
    try:
        social_profile_str = fed_user.osiris.get_profile(credential)
        social_profile = json.loads(social_profile_str)
        is_user_online = social_profile.get("online", False)
        if is_user_online:
            status = StatusCode.USER_ONLINE
            reason = "User is online"
            error_msg = f"Logic Error for {credential}: {status} - {reason}"
            log_to_file(error_msg, 'error')
            result['status'] = status
            result['reason'] = reason
            return json.dumps(result)
            
    except urllib.error.HTTPError as e:
        status = e.code
        reason = str(e.read())
        error_msg = f"HTTPError for {credential}: {status} - {reason}"
        log_to_file(error_msg, 'error')
        result['status'] = status
        result['reason'] = reason
        return json.dumps(result)
    except Exception as e:
        status = StatusCode.ERROR
        reason = Utils.exception_to_string(e)
        error_msg = f"Exception for {credential}: {reason}"
        log_to_file(error_msg, 'error')
        result['status'] = status
        result['reason'] = reason
        raise

    
    # Then change profile base on fields data
    try:
        profile_obj = {}
				
        for field in fields:
            field_str = field.get('field', "")
            if not field_str:
                continue
                
            field_type = field.get('field_type', FieldType.NORMAL)
            use_start_backup = field.get('use_start_backup', False)
            
            # Handle RESTORE and OBJECT types (both restore from start_date backup)
            if use_start_backup or field_type in (FieldType.RESTORE, FieldType.OBJECT):
                # For RESTORE and OBJECT fields, use the old value from start date
                backup_value_start = field.get('backup_value_start')
                if backup_value_start is not None:
                    # If the value is a dict/list (nested object), it will restore all sub-fields
                    # The batch_set operation will set the entire object at the specified path
                    profile_obj[field_str] = backup_value_start
                    value_type_name = type(backup_value_start).__name__
                    field_type_name = field_type if isinstance(field_type, str) else 'UNKNOWN'
                    log_msg = f"Restoring field {field_str} (type: {field_type_name}) with value type: {value_type_name}"
                    log_to_file(log_msg)
            elif field_type == FieldType.MILESTONE:
                # For milestone fields, use the max value from start, end, and current
                max_value = field.get('max_value')
                if max_value is not None:
                    profile_obj[field_str] = max_value
            else:
                # For normal fields, calculate compensation normally
                value = field.get('current_value', 0)
                value_add = field.get('value_to_compensate', 0)
                finally_value = value + value_add
                if value_add:                
                    profile_obj[field_str] = finally_value

        # Only update profile if there are fields to update
        if profile_obj:
            # Log the profile_obj structure for debugging nested objects
            log_msg = f"Updating profile with {len(profile_obj)} field(s). Fields: {list(profile_obj.keys())}"
            log_to_file(log_msg)
            for field_key, field_value in profile_obj.items():
                if isinstance(field_value, (dict, list)):
                    log_msg = f"Field {field_key} is a nested object (type: {type(field_value).__name__}) - will restore all sub-fields"
                    log_to_file(log_msg)
            fed_user.seshat.set_profile(object=json.dumps(profile_obj), operation='batch_set', selector='', credential=credential)
            result['status'] = StatusCode.SUCCESS
            result['reason'] = "Profile updated successfully"
        else:
            result['status'] = StatusCode.SUCCESS
            result['reason'] = "No fields to update"
            
    except urllib.error.HTTPError as e:
        status = e.code
        reason = str(e.read())
        error_msg = f"HTTPError for {credential}: {status} - {reason}"
        log_to_file(error_msg, 'error')
        result['status'] = status
        result['reason'] = reason
    except Exception as e:
        status = StatusCode.ERROR
        reason = Utils.exception_to_string(e)
        error_msg = f"Exception for {credential}: {reason}"
        log_to_file(error_msg, 'error')
        result['status'] = status
        result['reason'] = reason
        raise

    return json.dumps(result)

def compare_current_profile_with_backup(_credential, fields_to_compare, fed_user, _start_date, _end_date, path='', device=''):
    """Compare current profile with backup. This function will be run in TaskPool."""
    log_msg = f"[PID: {os.getpid()}] Starting profile comparison for credential: {_credential}"
    log_to_file(log_msg)
    
    result = {'credential': _credential}
    status = StatusCode.SUCCESS
    reason = 'OK'
    
    try:
        # Get current profile
        profile_now_str = fed_user.seshat.get_profile(
            selector=path, 
            credential=_credential, 
            include_fields=convert_field_list_to_string(fields_to_compare)
        )
        profile_now = json.loads(profile_now_str)
        
        start_date_backup = get_backup_profile_for_day(fed_user=fed_user, cred=_credential, day=_start_date)
        end_date_backup = get_backup_profile_for_day(fed_user=fed_user, cred=_credential, day=_end_date)

        if start_date_backup is None or end_date_backup is None:
            result['status'] = StatusCode.NOT_FOUND
            result['reason'] = 'Missing backup profile'
            return json.dumps(result)
        
        # Compare fields
        comparison_results = []
        for field_item in fields_to_compare:
            # Support both old format (string) and new format (dict)
            if isinstance(field_item, dict):
                field = field_item.get('field', '')
                field_type = field_item.get('type', FieldType.NORMAL)
            else:
                field = str(field_item)
                field_type = FieldType.NORMAL
            
            compare_result = compare_field(
                field=field,
                field_type=field_type,
                current_profile=profile_now, 
                backed_up_profile_start=start_date_backup,
                backed_up_profile_end=end_date_backup
            )
            comparison_results.append(compare_result)
        
        result['comparison_results'] = comparison_results
        result['profile_current'] = profile_now
        result['profile_backup_start'] = start_date_backup
        result['profile_backup_end'] = end_date_backup
        
    except urllib.error.HTTPError as e:
        status = e.code
        reason = str(e.read())
        error_msg = f"HTTPError for {_credential}: {status} - {reason}"
        log_to_file(error_msg, 'error')
        result['status'] = status
        result['reason'] = reason
    except Exception as e:
        status = StatusCode.ERROR
        reason = Utils.exception_to_string(e)
        error_msg = f"Exception for {_credential}: {reason}"
        log_to_file(error_msg, 'error')
        result['status'] = status
        result['reason'] = reason
        raise

    used_access_token = fed_user.janus.get_access_token_full_object()
    result['access_token'] = used_access_token   
    result['status'] = status
    result['reason'] = reason
    
    # Log completion
    completion_msg = f'Completed processing for {_credential} - Status: {status}'
    log_to_file(completion_msg)
    
    return json.dumps(result)

def get_backup_profile_for_day(fed_user, cred, day):
    """Get backup profile for a specific day."""
    start = f"{day} 00:00:00Z"
    end = f"{day} 23:59:59Z"
    
    backups = fed_user.seshat.get_profile_backups(
        credential=cred, 
        start_date=start, 
        end_date=end
    )
    backup_list = json.loads(backups)
    
    if not backup_list:
        error_msg = f"No backups found for credential: {cred}"
        log_to_file(error_msg, 'warning')
        return None
    
    # Sort the backups in descending order based on the 'date' field
    sorted_backups = sorted(
        backup_list,
        key=lambda x: x['date'],
        reverse=True  # Sort from latest to oldest
    )

    # Use the first (most recent) backup after sorting
    last_backup = sorted_backups[0]
    backup_id = last_backup['id']
    backup_profile_str = fed_user.seshat.get_profile_backup_by_id(
        credential=cred, 
        name='myprofile', 
        id=backup_id
    )
    backup_profile = json.loads(backup_profile_str)
    
    return backup_profile

def compare_field(field, field_type, current_profile, backed_up_profile_start, backed_up_profile_end):
    """Compare a specific field between current and backup profiles.
    
    Args:
        field: Field path string (e.g., '_pvp_stats.scratchpad.elo')
        field_type: Field type constant (FieldType.NORMAL, FieldType.RESTORE, FieldType.MILESTONE, FieldType.OBJECT)
        current_profile: Current profile dictionary
        backed_up_profile_start: Backup profile from start date
        backed_up_profile_end: Backup profile from end date
    """
    # Split the field path by dots to navigate the nested structure
    field_parts = field.split('.')
        
    def get_value(profile):
        val = profile
        for part in field_parts:
            if isinstance(val, dict):
                val = val.get(part, None)
            else:
                val = None
                break
        return val
        
    start_value = get_value(backed_up_profile_start)
    end_value = get_value(backed_up_profile_end)
    current_value = get_value(current_profile)
    
    # Determine if this field should use start_date backup value directly
    use_start_backup = field_type == FieldType.RESTORE
    use_object_restore = field_type == FieldType.OBJECT
    use_max_value = field_type == FieldType.MILESTONE
    value_to_compensate = 0
    max_value = None
    
    # Determine should_fix based on field type
    if use_start_backup or use_object_restore:
        # For RESTORE and OBJECT fields, should_fix if current differs from start backup
        # For nested objects (dict/list), use JSON comparison to ensure all sub-fields are compared
        if isinstance(start_value, (dict, list)) and isinstance(current_value, (dict, list)):
            # Use JSON comparison for nested objects to ensure all sub-fields are compared
            should_fix = (start_value is not None and current_value is not None and 
                         json.dumps(start_value, sort_keys=True) != json.dumps(current_value, sort_keys=True))
        else:
            should_fix = (start_value is not None and current_value is not None and 
                         start_value != current_value)
    elif use_max_value:
        # For milestone fields, use max value from start, end, and current
        values_to_compare = [v for v in [start_value, end_value, current_value] if v is not None]
        if values_to_compare:
            max_value = max(values_to_compare)
            should_fix = (current_value is None or current_value != max_value)
        else:
            max_value = None
            should_fix = False
    else:
        # For normal fields, should_fix if value_to_compensate > 0
        value_to_compensate = start_value - end_value if (start_value is not None and end_value is not None) else None
        should_fix = value_to_compensate > 0 if value_to_compensate is not None else False

    # Compare values
    comparison_result = {
        'field': field,
        'field_type': field_type,
        'current_value': current_value,
        'backup_value_start': start_value,
        'backup_value_end': end_value,
        'should_fix': should_fix,
        'value_to_compensate': value_to_compensate,
        'use_start_backup': use_start_backup or use_object_restore,  # Both RESTORE and OBJECT use start backup
        'max_value': max_value if use_max_value else None
    }
    
    return comparison_result

def prepare_fed_user(client_id, dc):       
    """Prepare FedWrapper user."""
    log_msg = f"Preparing FedWrapper user with client_id: {client_id}, dc: {dc}"
    logger.info(log_msg)
    log_to_file(log_msg)
    
    fed_user = FedWrapper.FedWrapper()  
    admin_username = config.ADMIN_USERNAME
    admin_password = config.ADMIN_PASSWORD
    scopes = config.SCOPES_LIST + ' storage_backup' 
    device = ''
    device_info = None
    
    if device != '':
       device_info = Utils.prepare_device_info(device)
    
    try:
        fed_user.initialize(
            client_id=client_id, 
            datacenter=dc, 
            pandora_url='', 
            credential=admin_username, 
            password=admin_password, 
            for_credential='', 
            scopes=scopes, 
            access_token='', 
            device_info=device_info
        )
        log_msg = f"Successfully initialized FedWrapper user"
        logger.info(log_msg)
        log_to_file(log_msg)
        
    except Exception as e:
        error_msg = f'Failed to get access token on {client_id} {dc}: {str(e)}'
        logger.exception(error_msg)
        log_to_file(error_msg, 'error')
        raise
    
    return fed_user

def convert_field_list_to_string(field_list):
    """Convert a list of fields to a comma-separated string.
    
    Supports both formats:
    - List of strings: ['field1', 'field2'] -> 'field1,field2'
    - List of dicts: [{'field': 'field1', 'type': 'NORMAL'}, ...] -> 'field1,field2'
    """
    field_names = []
    for item in field_list:
        if isinstance(item, dict):
            field_names.append(item.get('field', ''))
        else:
            field_names.append(str(item))
    return ",".join(field_names)

def process_check_users_with_taskpool(data, pid, dc, fed_user, fields_to_compare, start_date, end_date, dry_run=True):
    """Process multiple users for profile comparison using TaskPool.
    
    Args:
        data: User data dictionary or list
        pid: Platform ID
        dc: Datacenter
        fed_user: FedWrapper user instance
        fields_to_compare: List of fields to compare
        start_date: Start date string
        end_date: End date string
        dry_run: If True, skip compensation execution (check logic still runs)
    """
    total_users = 0
    jobs = []

    # START TIMING
    start_time = time.time()
    logger.info(f"TaskPool processing START TIME: {time.strftime('%Y-%m-%d %H:%M:%S')}")

    # Check if data structure matches your example
    if dc in data and pid in data[dc]:
        users_dict = data[dc][pid]
        total_users = len(users_dict)
    elif isinstance(data, list):
        # If data is just a list of credentials
        users_dict = {cred: None for cred in data}
        total_users = len(users_dict)
    else:
        error_msg = f"Unexpected data structure. Expected dict with dc/pid keys or list of credentials."
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        return
    
    log_msg = f"Starting to process {total_users} users using TaskPool"
    logger.info(log_msg)
    log_to_file(log_msg)
    
    # Prepare list of already processed credentials (if any)
    processed_credentials = set()   

    # Create tasks for TaskPool
    for cred in users_dict.keys():
        if cred in processed_credentials:
            continue
        
        # Add task to TaskPool
        jobs += TaskPool.add_task(
            compare_current_profile_with_backup,
            (cred, 
             fields_to_compare, 
             fed_user, 
             start_date, 
             end_date),
            [],
            get_result=True
        )
        # log_to_file(f"Added task for credential: {cred}")
    
    # Wait for all tasks to complete and get results
    logger.info(f"Waiting for {len(jobs)} tasks to complete...")
    
    results = TaskPool.wait_and_get_result(jobs)
    
    # DEBUG CHECK:
    print(f"\n=== DEBUG INFO ===")
    print(f"Total results received: {len(results)}")
    print(f"Sample first result: {results[0][:200] if results else 'No results'}")
    
    successful_count = 0
    for r in results:
        try:
            res = json.loads(r)
            if res.get('status') == StatusCode.SUCCESS:
                successful_count += 1
        except (json.JSONDecodeError, KeyError):
            pass
    
    print(f"Successful results: {successful_count}/{len(results)}")
    print(f"=== END DEBUG ===\n")

    # Prepare lists for output
    compensation_data = {}

    # Write results to output files using CSV module
    successful = 0
    failed = 0
    
    with open(OUTPUT_FILE_CHECK, 'w', newline='', encoding='utf-8') as fd_res, \
         open(OUTPUT_FILE_ERRORS, 'w', newline='', encoding='utf-8') as fd_err:
        
        # Setup CSV writers
        check_writer = csv.writer(fd_res)
        error_writer = csv.writer(fd_err)
        
        # Write headers
        check_writer.writerow(['credential', 'field', 'field_type', 'current_value', 
                              'backup_value_start', 'backup_value_end', 'should_fix', 'value_to_compensate', 'max_value'])
        error_writer.writerow(['credential', 'status', 'reason'])
        
        for r in results:
            try:
                res = json.loads(r)
                cred = res.get('credential', 'unknown')
                
                if res.get('status') == StatusCode.SUCCESS:
                    successful += 1
                    # Write each field comparison result
                    comparison_results = res.get('comparison_results', [])
                    for comp in comparison_results:
                        field_type = comp.get('field_type', FieldType.NORMAL)
                        check_writer.writerow([
                            cred,
                            comp['field'],
                            field_type,
                            comp['current_value'],
                            comp['backup_value_start'],
                            comp['backup_value_end'],
                            comp['should_fix'],
                            comp['value_to_compensate'],
                            comp.get('max_value')
                        ])
                        
                        # Store compensation data if needed
                        if comp.get('should_fix', False):
                            if cred not in compensation_data:
                                compensation_data[cred] = []
                            compensation_data[cred].append({
                                'field': comp['field'],
                                'field_type': comp.get('field_type', FieldType.NORMAL),
                                'should_fix': True,
                                'current_value': comp['current_value'],
                                'value_to_compensate': comp['value_to_compensate'],
                                'backup_value_start': comp.get('backup_value_start'),
                                'use_start_backup': comp.get('use_start_backup', False),
                                'max_value': comp.get('max_value')
                            })
                else:
                    failed += 1
                    error_writer.writerow([
                        cred,
                        res.get('status', 'unknown'),
                        res.get('reason', 'unknown')
                    ])
                    
            except json.JSONDecodeError as e:
                error_msg = f"Error parsing JSON result: {str(e)}"
                logger.error(error_msg)
                log_to_file(error_msg, 'error')
                error_writer.writerow(['unknown', StatusCode.ERROR, str(e)])
                failed += 1
            except Exception as e:
                error_msg = f"Error processing result: {str(e)}"
                logger.error(error_msg)
                log_to_file(error_msg, 'error')
                error_writer.writerow(['unknown', StatusCode.ERROR, str(e)])
                failed += 1
    
    completion_msg = f"Completed check processing {total_users} users. Successful: {successful}, Failed: {failed}"
    logger.info(completion_msg)
    log_to_file(completion_msg)

    # Process through compensation list
    # Write compensation data to JSON file
    if compensation_data:
        with open(COMPENSATION_LIST, 'w') as fd_comp:
            json.dump(compensation_data, fd_comp, indent=2)
            
        completion_msg = (
            f"Stored {len(compensation_data)} credentials "
            f"with compensation needed in {COMPENSATION_LIST}"
        )
        logger.info(completion_msg)
        log_to_file(completion_msg)
    
    # Skip compensation execution if dry-run mode is enabled
    if dry_run:
        # END TIMING (for check phase)
        end_time = time.time()
        logger.info(f"TaskPool processing END TIME: {time.strftime('%Y-%m-%d %H:%M:%S')}")
        total_time = end_time - start_time
        
        dry_run_msg = (
            f"DRY-RUN MODE: Skipping compensation execution. "
            f"Found {len(compensation_data)} credentials that need compensation. "
            f"Compensation data saved to {COMPENSATION_LIST}"
        )
        logger.info(dry_run_msg)
        log_to_file(dry_run_msg)
        
        # Print summary
        print(f"\n=== DRY-RUN MODE ===")
        print(dry_run_msg)
        print(f"\n=== CHECK PROCESSING COMPLETE ===")
        print(f"Total time: {total_time:.2f} seconds")
        print(f"Total users: {total_users}")
        print(f"Successful: {successful}")
        print(f"Failed: {failed}")
        print(f"Results saved to: {OUTPUT_FILE_CHECK}")
        print(f"Errors saved to: {OUTPUT_FILE_ERRORS}")
        print(f"Compensation list saved to: {COMPENSATION_LIST}")
        print(f"Log file: {LOG_FILE}")
        print(f"=== END DRY-RUN ===\n")
        return
    
    # Execute compensation
    compensate_jobs = []
    log_msg = f"Starting to process {len(compensation_data)} compensation tasks using TaskPool"
    logger.info(log_msg)
    log_to_file(log_msg)
    for credential, fields in compensation_data.items():
        compensate_jobs += TaskPool.add_task(
            process_compensate_user,
            (credential, 
             fields,
             fed_user),
            [],
            get_result=True
        )
    compensate_results = TaskPool.wait_and_get_result(compensate_jobs)    
    # Process compensation results
    successful_compensation = 0
    failed_compensation = 0
    # Write results to output files using CSV module
    with open(OUTPUT_COMPENSATE_FAILED, 'w', newline='', encoding='utf-8') as cp_res:
        failed_writer = csv.writer(cp_res)
        failed_writer.writerow(['credential', 'status', 'reason'])
        
        for r in compensate_results:
            try:
                res = json.loads(r)
                cred = res.get('credential', 'unknown')
                if res.get('status') == StatusCode.SUCCESS:
                    successful_compensation += 1
                else:
                    failed_compensation += 1
                    failed_writer.writerow([
                        cred,
                        res.get('status', 'unknown'),
                        res.get('reason', 'unknown')
                    ])
                    error_msg = f"Compensation failed for {cred} with status: {res.get('status', 'unknown')}"
                    logger.error(error_msg)
                    log_to_file(error_msg, 'error')
            except json.JSONDecodeError as e:
                error_msg = f"Error parsing compensation result JSON: {str(e)}"
                logger.error(error_msg)
                log_to_file(error_msg, 'error')
                failed_writer.writerow(['unknown', StatusCode.ERROR, str(e)])
                failed_compensation += 1
            except Exception as e:
                error_msg = f"Error processing compensation result: {str(e)}"
                logger.error(error_msg)
                log_to_file(error_msg, 'error')
                failed_writer.writerow(['unknown', StatusCode.ERROR, str(e)])
                failed_compensation += 1

    # Log compensation results
    completion_msg = (
        f"Compensation completed for {successful_compensation}/{len(compensate_results)} credentials."
    )
    logger.info(completion_msg)
    log_to_file(completion_msg)

    if failed_compensation > 0:
        error_msg = f"Failed to compensate {failed_compensation} credentials."
        logger.error(error_msg)
        log_to_file(error_msg, 'error')

    # END TIMING
    end_time = time.time()
    logger.info(f"TaskPool processing END TIME: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    total_time = end_time - start_time
    # Also log summary to console
    print(f"\n=== PROCESSING COMPLETE ===")
    print(f"Total time: {total_time:.2f} seconds")
    print(f"Total users: {total_users}")
    print(f"Successful: {successful}")
    print(f"Failed: {failed}")
    print(f"Results saved to: {OUTPUT_FILE_CHECK}")
    print(f"Errors saved to: {OUTPUT_FILE_ERRORS}")
    print(f"Log file: {LOG_FILE}")

def generate_simulated_credentials(count=500):
    """Generate simulated credentials."""
    import uuid
    real_creds = [
        "fed_id:0cb6edb8-098a-11e7-abaf-b8ca3a60b6e4",
        "fed_id:ca1ffbf6-77d7-11eb-902c-b8ca3a603534",
        "fed_id:d69a00f2-b6f7-11f0-9e83-b8ca3a660720",
        "fed_id:ac71b19e-e5b2-11e6-b11c-b8ca3a603534",
        "fed_id:e9b197d4-d742-11f0-a6db-b8ca3a660720"
    ]
    simulated = real_creds.copy()
    for i in range(count - len(real_creds)):
        simulated.append(f"fed_id:{uuid.uuid4()}")
    return simulated

##################################################################################################

if __name__ == "__main__":
    # Log script start
    start_msg = "Script started"
    logger.info(start_msg)
    log_to_file(start_msg)
    log_to_file(f"Log file: {LOG_FILE}")
    log_to_file(f"Output file: {OUTPUT_FILE_CHECK}")
    log_to_file(f"Errors file: {OUTPUT_FILE_ERRORS}")
    
    # Validate configuration
    is_valid, error_msg = validate_config(CONFIG)
    if not is_valid:
        error_msg_full = f"Configuration validation failed: {error_msg}"
        logger.error(error_msg_full)
        log_to_file(error_msg_full, 'error')
        print(f"ERROR: {error_msg_full}")
        sys.exit(1)
    
    # Config (edit values in CONFIG at the top of the file)
    dry_run = CONFIG['dry_run']
    get_compensation = CONFIG['get_compensation']
    run_test = CONFIG['run_test']
    pid = CONFIG['pid']
    dc = CONFIG['dc']
    fields_to_check = CONFIG['fields_to_check']
    start_date = CONFIG['start_date']
    end_date = CONFIG['end_date']
    
    # Log dry-run mode if enabled
    if dry_run:
        dry_run_msg = "DRY-RUN MODE: Compensation execution will be skipped"
        logger.info(dry_run_msg)
        log_to_file(dry_run_msg)
    
    # Load user list from file
    log_to_file(f"Loading data from: {INPUT_FILE}")
    data = safe_load_json(INPUT_FILE)
    # data = generate_simulated_credentials()

    if data:
        if isinstance(data, dict):
            data_size = "dict with keys: " + ", ".join(data.keys())
        elif isinstance(data, list):
            data_size = f"list with {len(data)} items"
        else:
            data_size = str(type(data))
        
        log_msg = f"Successfully loaded data: {data_size}"
        logger.info(log_msg)
        log_to_file(log_msg)
    else:
        error_msg = "Failed to load JSON data from input file"
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        sys.exit(1)
    
    # Choose client id
    clientId = chose_client_id(pid, dc)
    if not clientId:
        error_msg = f"No client ID found for pid={pid}, dc={dc}"
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        sys.exit(1)
    
    log_msg = f"Using client id: {clientId}"
    logger.info(log_msg)
    log_to_file(log_msg)
    
    # Prepare fed user
    try:
        fed_user = prepare_fed_user(client_id=clientId, dc=dc)
    except Exception as e:
        error_msg = f"Failed to prepare FedWrapper user: {str(e)}"
        logger.error(error_msg)
        log_to_file(error_msg, 'error')
        sys.exit(1)
    
    # Process users with TaskPool to generate compensation_needed.json
    if get_compensation:
        try:
            process_check_users_with_taskpool(
                data=data,
                pid=pid,
                dc=dc,
                fed_user=fed_user,
                fields_to_compare=fields_to_check,
                start_date=start_date,
                end_date=end_date,
                dry_run=dry_run
            )
        except Exception as e:
            error_msg = f"Error during processing: {str(e)}"
            logger.exception(error_msg)
            log_to_file(error_msg, 'error')
            sys.exit(1)

    # Optionally run test compensation from COMPENSATION_LIST
    if run_test:
        log_to_file(f"Loading compensation test data from: {COMPENSATION_LIST}")
        test_data = safe_load_json(COMPENSATION_LIST)
        if not test_data:
            log_to_file(f"No test compensation data found in {COMPENSATION_LIST}", 'warning')
        else:
            test_result = {}
            for credential, fields in test_data.items():
                result = process_compensate_user(
                    credential=credential,
                    fields=fields,
                    fed_user=fed_user
                )
                test_result[credential] = result
            log_to_file(f"Test compensation applied for {len(test_result)} credential(s) from {COMPENSATION_LIST}")

    # Log script completion
    end_msg = "Script completed successfully"
    logger.info(end_msg)
    log_to_file(end_msg)