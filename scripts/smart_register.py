#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Smart Auto Register - Intelligent Form Filling
Tự động phát hiện và điền TẤT CẢ các trường form đăng ký.

Install:
    pip install playwright faker --break-system-packages
    playwright install chromium
"""

import asyncio
import json
import argparse
import sys
import random
import string
import re
import os
import subprocess
from datetime import datetime, timedelta
from typing import Optional, Dict, List, Tuple

try:
    from playwright.async_api import async_playwright, Page, ElementHandle
    from faker import Faker
except ImportError:
    print("[ERROR] Missing dependencies.")
    print("  pip install playwright faker --break-system-packages")
    print("  playwright install chromium")
    sys.exit(1)

# Faker với locale Việt Nam + US
fake = Faker(['vi_VN', 'en_US'])

# ══════════════════════════════════════════════════════════════════════════════
#  GLOBAL MODAL WATCHDOG - Auto-close modal khi script treo
# ══════════════════════════════════════════════════════════════════════════════
async def _try_close_modal_global(page: Page):
    """Global helper to close modal/notification - work everywhere"""
    try:
        # Step 1: Escape key
        await page.keyboard.press('Escape')
        await asyncio.sleep(0.3)
    except:
        pass
    
    # Step 2: CSS-based selectors for common patterns
    try:
        css_patterns = [
            "[id*='close'], [id*='dismiss'], [id*='cancel']",
            "[class*='close-btn'], [class*='dismiss-btn'], [class*='cancel-btn']",
            "[class*='modal-close'], [class*='modal-dismiss'], [class*='modal-cancel']",
            ".close, .dismiss, .cancel, .overlay-close",
            "[aria-label*='close' i], [aria-label*='dismiss' i], [aria-label*='cancel' i]",
            "button.btn-close, button.close-button, button.exit-button",
        ]
        
        for pattern in css_patterns:
            try:
                elements = await page.query_selector_all(pattern)
                for el in elements:
                    if await el.is_visible():
                        await el.click(timeout=2000)
                        await asyncio.sleep(0.3)
                        return
            except:
                continue
    except:
        pass
    
    # Step 3: JavaScript-based button text matching
    try:
        dismiss_keywords = [
            "dismiss", "close", "cancel", "skip", "no", "ok", "confirm",
            "continue", "next", "got it", "understood", "agree", "accept",
            "later", "maybe later", "ignore", "decline", "refuse",
            "thanks", "no thanks", "done", "x", "exit",
        ]
        
        find_and_click_js = f"""
        (function() {{
            const keywords = {json.dumps(dismiss_keywords)};
            const pattern = new RegExp(keywords.join('|'), 'i');
            const buttons = document.querySelectorAll('button, [role="button"], input[type="button"]');
            for (const btn of buttons) {{
                const text = btn.innerText || btn.textContent || btn.value || '';
                if (pattern.test(text.trim())) {{
                    if (btn.offsetParent !== null) {{
                        btn.click();
                        return true;
                    }}
                }}
            }}
            return false;
        }})()
        """
        
        result = await page.evaluate(find_and_click_js)
        if result:
            await asyncio.sleep(0.3)
            return
    except:
        pass
    
    # Step 4: Fallback - Click at viewport center
    try:
        viewport = page.viewport_size
        if viewport:
            x = viewport['width'] // 6 * 5
            y = viewport['height'] // 4 * 3
            await page.mouse.click(x, y)
            await asyncio.sleep(0.3)
    except:
        pass

async def _wait_with_modal_timeout(operation_coro, timeout: int = 10, page: Optional[Page] = None, retry: bool = True):
    """
    Wrapper cho các operation có thể treo (page.goto, page.fill, v.v.)
    Nếu timeout, tự động gọi _try_close_modal_global và retry
    """
    try:
        result = await asyncio.wait_for(operation_coro, timeout=timeout)
        return result
    except asyncio.TimeoutError:
        if page:
            await _try_close_modal_global(page)
            await asyncio.sleep(1)
            
            if retry:
                try:
                    result = await asyncio.wait_for(operation_coro, timeout=timeout)
                    return result
                except asyncio.TimeoutError:
                    raise
        else:
            raise
    except Exception as e:
        raise

# ══════════════════════════════════════════════════════════════════════════════
#  REGISTER PAGE FINDER - REGEX-BASED
# ══════════════════════════════════════════════════════════════════════════════
class RegisterPageFinder:
    """Tìm endpoint register sử dụng regex pattern matching"""
    
    REGISTER_PATTERNS = [
        "/register", "register",
        "/registration", "registration",
        "/signup", "signup",
        "/sign-up", "sign-up",
        "/dang-ky", "dang-ky",
        "/dangky", "dangky",
        "/đăng-ký", "đăng-ký",
    ]
    
    def __init__(self, base_url: str):
        self.base_url = base_url.rstrip("/").split('#')[0].rstrip('/')
    
    async def _find_register_elements_by_regex(self, page: Page) -> List[Tuple]:
        """Dùng regex tìm các element điều hướng có href/id/class/text chứa 'register/signup'"""
        results = []
        try:
            # Duyệt các element có thể click: link, button, element có role=button
            elements = await page.query_selector_all("a, button, input[type='button'], input[type='submit'], [role='button'], [role='link']")
            
            for idx, el in enumerate(elements):
                try:
                    # Get element attributes
                    el_href = await el.get_attribute("href") or ""
                    el_id = await el.get_attribute("id") or ""
                    el_class = await el.get_attribute("class") or ""
                    el_aria = await el.get_attribute("aria-label") or ""
                    el_text = (await el.text_content() or "").strip()
                    
                    # Check if visible
                    is_visible = await el.is_visible()
                    
                    # Combine all attributes for pattern matching
                    combined = f"{el_href} {el_id} {el_class} {el_aria} {el_text}".lower()
                    
                    # Match any register pattern
                    matched_pattern = None
                    for pattern in self.REGISTER_PATTERNS:
                        if pattern.lower() in combined:
                            matched_pattern = pattern
                            break
                    
                    if matched_pattern:
                        match_info = f"pattern={matched_pattern}"
                        if el_href:
                            match_info = f"href={el_href}"
                        results.append((el, match_info, is_visible))
                
                except:
                    continue
        
        except Exception as e:
            pass
        
        return results

# ══════════════════════════════════════════════════════════════════════════════
#  INTELLIGENT FIELD DETECTOR & FILLER
# ══════════════════════════════════════════════════════════════════════════════

class SmartFormFiller:
    """AI-powered form field detector and filler"""
    
    # Field type patterns (name, placeholder, label, id, class)
    FIELD_PATTERNS = {
        'email': [
            'email', 'e-mail', 'mail', 'correo', 'mel',
            'dia chi email', 'địa chỉ email'
        ],
        'username': [
            'username', 'user', 'login', 'userid', 'user_name',
            'ten dang nhap', 'tên đăng nhập', 'tai khoan', 'tài khoản'
        ],
        'password': [
            'password', 'pass', 'pwd', 'passwd', 'mat khau', 'mật khẩu'
        ],
        'confirm_password': [
            'confirm', 'repeat', 'retype', 'again', 're-enter',
            'xac nhan', 'xác nhận', 'nhap lai', 'nhập lại'
        ],
        'firstname': [
            'firstname', 'first_name', 'first-name', 'fname', 'given',
            'ten', 'tên', 'ho ten', 'họ tên', 'name'
        ],
        'lastname': [
            'lastname', 'last_name', 'last-name', 'lname', 'surname', 'family',
            'ho', 'họ'
        ],
        'fullname': [
            'fullname', 'full_name', 'full-name', 'name', 'realname',
            'ho ten', 'họ tên', 'ho va ten', 'họ và tên'
        ],
        'phone': [
            'phone', 'mobile', 'tel', 'telephone', 'cell', 'cellphone',
            'dien thoai', 'điện thoại', 'so dien thoai', 'số điện thoại'
        ],
        'address': [
            'address', 'street', 'addr', 'location',
            'dia chi', 'địa chỉ', 'duong', 'đường'
        ],
        'city': [
            'city', 'town', 'thanh pho', 'thành phố', 'tinh', 'tỉnh'
        ],
        'country': [
            'country', 'nation', 'quoc gia', 'quốc gia'
        ],
        'zipcode': [
            'zip', 'zipcode', 'postal', 'postcode', 'ma buu dien', 'mã bưu điện'
        ],
        'company': [
            'company', 'organization', 'org', 'business',
            'cong ty', 'công ty', 'to chuc', 'tổ chức'
        ],
        'website': [
            'website', 'site', 'url', 'web', 'homepage',
            'trang web', 'web site'
        ],
        'birthday': [
            'birthday', 'birth', 'dob', 'date_of_birth', 'birthdate',
            'ngay sinh', 'ngày sinh', 'sinh nhat', 'sinh nhật'
        ],
        'age': [
            'age', 'tuoi', 'tuổi'
        ],
        'gender': [
            'gender', 'sex', 'gioi tinh', 'giới tính'
        ],
        'title': [
            'title', 'salutation', 'prefix', 'danh xung', 'chức danh'
        ],
        'occupation': [
            'occupation', 'job', 'profession', 'work',
            'nghe nghiep', 'nghề nghiệp', 'cong viec', 'công việc'
        ],
    }
    
    def __init__(self, credentials: Dict):
        self.creds = credentials
        self.filled_fields = []
    
    def _normalize_text(self, text: str) -> str:
        """Normalize text for pattern matching"""
        if not text:
            return ""
        # Remove accents, lowercase, remove special chars
        text = text.lower()
        text = re.sub(r'[^a-z0-9\s]', '', text)
        text = re.sub(r'\s+', '', text)
        return text
    
    def _detect_field_type(self, element_info: Dict) -> Optional[str]:
        """Detect field type from name, placeholder, label, id, class"""
        # Combine all attributes
        all_text = ' '.join([
            element_info.get('name', ''),
            element_info.get('placeholder', ''),
            element_info.get('label', ''),
            element_info.get('id', ''),
            element_info.get('class', ''),
            element_info.get('aria-label', ''),
        ]).lower()
        
        normalized = self._normalize_text(all_text)
        
        # Check each pattern
        for field_type, patterns in self.FIELD_PATTERNS.items():
            for pattern in patterns:
                pattern_norm = self._normalize_text(pattern)
                if pattern_norm in normalized:
                    return field_type
        
        return None
    
    def _generate_value(self, field_type: str, input_type: str = 'text') -> str:
        """Generate appropriate fake data for field type"""
        
        # Use provided credentials
        if field_type == 'email':
            return self.creds['email']
        elif field_type == 'username':
            return self.creds['username']
        elif field_type == 'password' or field_type == 'confirm_password':
            return self.creds['password']
        
        # Generate fake data
        elif field_type == 'firstname':
            return fake.first_name()
        elif field_type == 'lastname':
            return fake.last_name()
        elif field_type == 'fullname':
            return fake.name()
        elif field_type == 'phone':
            return fake.phone_number()[:15]  # Limit length
        elif field_type == 'address':
            return fake.street_address()
        elif field_type == 'city':
            return fake.city()
        elif field_type == 'country':
            return fake.country()
        elif field_type == 'zipcode':
            return fake.postcode()
        elif field_type == 'company':
            return fake.company()
        elif field_type == 'website':
            return fake.url()
        elif field_type == 'birthday':
            # Date input
            birth_date = fake.date_of_birth(minimum_age=18, maximum_age=70)
            if input_type == 'date':
                return birth_date.strftime('%Y-%m-%d')
            else:
                return birth_date.strftime('%d/%m/%Y')
        elif field_type == 'age':
            return str(random.randint(18, 65))
        elif field_type == 'gender':
            return random.choice(['Male', 'Female', 'Nam', 'Nữ', 'M', 'F'])
        elif field_type == 'title':
            return random.choice(['Mr', 'Ms', 'Mrs', 'Dr', 'Ông', 'Bà'])
        elif field_type == 'occupation':
            return fake.job()
        
        # Default: random text
        return fake.word()
    
    async def _get_element_info(self, element: ElementHandle) -> Dict:
        """Extract all info from an input element"""
        try:
            info = {
                'name': await element.get_attribute('name') or '',
                'id': await element.get_attribute('id') or '',
                'class': await element.get_attribute('class') or '',
                'placeholder': await element.get_attribute('placeholder') or '',
                'type': await element.get_attribute('type') or 'text',
                'aria-label': await element.get_attribute('aria-label') or '',
                'required': await element.get_attribute('required') is not None,
            }
            
            # Try to find associated label
            try:
                label = None
                # By ID
                if info['id']:
                    label_el = await element.evaluate_handle(f'document.querySelector("label[for=\\"{info["id"]}\\"]")')
                    if label_el:
                        label = await label_el.inner_text()
                
                # By parent
                if not label:
                    parent = await element.evaluate_handle('el => el.closest("label")', element)
                    if parent:
                        label = await parent.inner_text()
                
                info['label'] = label or ''
            except:
                info['label'] = ''
            
            return info
        except:
            return {}
    
    async def fill_form(self, page: Page):
        """Intelligently fill all form fields"""
        
        print("[*] Analyzing form fields...")
        
        # Find all input fields
        input_selectors = [
            'input[type="text"]',
            'input[type="email"]',
            'input[type="tel"]',
            'input[type="url"]',
            'input[type="number"]',
            'input[type="date"]',
            'input[type="password"]',
            'input:not([type])',  # No type attribute
        ]
        
        all_inputs = []
        for selector in input_selectors:
            try:
                elements = await page.query_selector_all(selector)
                all_inputs.extend(elements)
            except:
                continue
        
        print(f"    → Found {len(all_inputs)} input fields")
        
        # Fill each field
        filled_count = 0
        for i, element in enumerate(all_inputs):
            try:
                # Check if visible
                is_visible = await element.is_visible()
                if not is_visible:
                    continue
                
                # Get element info
                info = await self._get_element_info(element)
                input_type = info.get('type', 'text')
                
                # Detect field type
                field_type = self._detect_field_type(info)
                
                if not field_type:
                    # If field not recognized, analyze it more thoroughly
                    field_name = info.get('name', '').lower()
                    field_id = info.get('id', '').lower()
                    
                    # Try to infer from name/id patterns
                    if any(x in field_name + field_id for x in ['email', 'mail', 'user']):
                        field_type = 'email'
                    elif any(x in field_name + field_id for x in ['password', 'pass']):
                        field_type = 'password'
                    elif any(x in field_name + field_id for x in ['confirm', 'repeat', 'retype']):
                        field_type = 'password'
                    elif any(x in field_name + field_id for x in ['question', 'answer', 'security', 'hint']):
                        field_type = 'fullname'  # Generic text field
                    elif input_type == 'date':
                        field_type = 'birthday'
                    elif input_type == 'tel':
                        field_type = 'phone'
                    elif input_type == 'number':
                        field_type = 'age'
                    else:
                        # Default to generic text
                        field_type = 'unknown'
                
                # Generate value
                value = self._generate_value(field_type, input_type)
                
                # Fill field
                await element.fill(value)
                filled_count += 1
                
                field_name = info.get('name') or info.get('id') or info.get('placeholder') or f'field_{i}'
                self.filled_fields.append({
                    'name': field_name,
                    'type': field_type,
                    'value': value if field_type not in ['password', 'confirm_password'] else '***'
                })
                
                print(f"    ✓ {field_name[:30]:30s} → {field_type}")
                
            except Exception as e:
                continue
        
        print(f"    → Filled {filled_count}/{len(all_inputs)} input fields")
        
        # Handle SELECT elements - comprehensive regex-based approach
        try:
            # 1. Handle standard HTML <select> elements
            try:
                selects = await page.query_selector_all('select')
                for idx, select in enumerate(selects):
                    if not await select.is_visible():
                        continue
                    
                    try:
                        options = await select.query_selector_all('option')
                        
                        if len(options) > 1:
                            start_idx = 0
                            first_text = (await options[0].text_content() or "").strip().lower()
                            
                            # Skip placeholder options
                            if any(p in first_text for p in ['select', 'choose', 'choose...', 'select...', '--', 'please']):
                                start_idx = 1
                            
                            available = options[start_idx:]
                            if available:
                                selected_option = random.choice(available)
                                value = await selected_option.get_attribute('value')
                                
                                if value and value.strip():
                                    await select.select_option(value)
                                    selected_value = value
                                else:
                                    text = await selected_option.text_content()
                                    if text and text.strip():
                                        await select.select_option(label=text.strip())
                                        selected_value = text.strip()
                                    else:
                                        opt_idx = list(available).index(selected_option) + start_idx
                                        await select.select_option(index=opt_idx)
                                        selected_value = f"index_{opt_idx}"
                                
                                print(f"    ✓ select[{idx}] (HTML) → {selected_value[:40]}")
                        elif len(options) == 1:
                            value = await options[0].get_attribute('value')
                            if value:
                                await select.select_option(value)
                            print(f"    ✓ select[{idx}] (HTML) → single option")
                    except Exception as e:
                        print(f"    ! select[{idx}] error: {str(e)[:50]}")
                        continue
            except:
                pass
            
            # 2. Handle Material Design <mat-select> components
            try:
                mat_selects = await page.query_selector_all('mat-select')
                for idx, mat_select in enumerate(mat_selects):
                    if not await mat_select.is_visible():
                        continue
                    
                    try:
                        await mat_select.click(timeout=3000)
                        await asyncio.sleep(0.8)
                        
                        # Find options in Material panel
                        mat_options = await page.query_selector_all('mat-option')
                        
                        if mat_options:
                            available_options = []
                            for opt in mat_options:
                                try:
                                    is_disabled = await opt.get_attribute('aria-disabled')
                                    if is_disabled != 'true':
                                        available_options.append(opt)
                                except:
                                    available_options.append(opt)
                            
                            if available_options:
                                selected_option = random.choice(available_options)
                                option_text = (await selected_option.text_content() or "").strip()
                                
                                await selected_option.click(timeout=3000)
                                await asyncio.sleep(0.5)
                                print(f"    ✓ select[{idx}] (mat-select) → {option_text[:40]}")
                            else:
                                print(f"    ! select[{idx}] (mat-select) → no available options")
                        else:
                            print(f"    ! select[{idx}] (mat-select) → no options found")
                    except Exception as e:
                        print(f"    ! select[{idx}] (mat-select) error: {str(e)[:50]}")
                        continue
            except:
                pass
            
            # 3. Handle ng-select (Angular custom select)
            try:
                ng_selects = await page.query_selector_all('ng-select')
                for idx, ng_select in enumerate(ng_selects):
                    if not await ng_select.is_visible():
                        continue
                    
                    try:
                        await ng_select.click(timeout=3000)
                        await asyncio.sleep(0.8)
                        
                        ng_options = await page.query_selector_all('[role="option"]')
                        
                        if ng_options:
                            available_options = [opt for opt in ng_options if await opt.is_visible()]
                            
                            if available_options:
                                selected_option = random.choice(available_options)
                                option_text = (await selected_option.text_content() or "").strip()
                                
                                await selected_option.click(timeout=3000)
                                await asyncio.sleep(0.5)
                                print(f"    ✓ select[{idx}] (ng-select) → {option_text[:40]}")
                            else:
                                print(f"    ! select[{idx}] (ng-select) → no available options")
                        else:
                            print(f"    ! select[{idx}] (ng-select) → no options found")
                    except Exception as e:
                        print(f"    ! select[{idx}] (ng-select) error: {str(e)[:50]}")
                        continue
            except:
                pass
            
            # 4. Handle generic combobox/dropdown elements by role attribute
            try:
                comboboxes = await page.query_selector_all('[role="combobox"]')
                processed = set()
                
                for idx, combobox in enumerate(comboboxes):
                    if not await combobox.is_visible():
                        continue
                    
                    # Skip if already processed (mat-select, ng-select already handled)
                    tag = await combobox.evaluate('el => el.tagName.toLowerCase()')
                    if tag in ['mat-select', 'ng-select']:
                        continue
                    
                    try:
                        element_id = await combobox.evaluate('el => el.id || el.className')
                        if element_id in processed:
                            continue
                        processed.add(element_id)
                        
                        await combobox.click(timeout=3000)
                        await asyncio.sleep(0.8)
                        
                        # Try multiple selectors for options
                        options = await page.query_selector_all('[role="option"], .option, [class*="option"]')
                        
                        if options:
                            available_options = [opt for opt in options if await opt.is_visible()]
                            
                            if available_options:
                                selected_option = random.choice(available_options)
                                option_text = (await selected_option.text_content() or "").strip()
                                
                                await selected_option.click(timeout=3000)
                                await asyncio.sleep(0.5)
                                print(f"    ✓ select[{idx}] (combobox) → {option_text[:40]}")
                            else:
                                print(f"    ! select[{idx}] (combobox) → no available options")
                        else:
                            print(f"    ! select[{idx}] (combobox) → no options found")
                    except Exception as e:
                        print(f"    ! select[{idx}] (combobox) error: {str(e)[:50]}")
                        continue
            except:
                pass
        
        except Exception as e:
            print(f"[!] Select handling error: {str(e)[:50]}")
        
        # Check all checkboxes (terms, agreements...)
        try:
            checkboxes = await page.query_selector_all('input[type="checkbox"]')
            for checkbox in checkboxes:
                if await checkbox.is_visible() and not await checkbox.is_checked():
                    await checkbox.check()
                    print(f"    ✓ checkbox → checked")
        except:
            pass
        
        # Radio buttons - select first in each group
        try:
            radios = await page.query_selector_all('input[type="radio"]')
            checked_groups = set()
            for radio in radios:
                if await radio.is_visible():
                    name = await radio.get_attribute('name')
                    if name and name not in checked_groups:
                        await radio.check()
                        checked_groups.add(name)
                        print(f"    ✓ radio ({name}) → selected")
        except:
            pass
        
        print(f"[✓] Filled {len(self.filled_fields)} fields")


# ══════════════════════════════════════════════════════════════════════════════
#  MAIN REGISTRATION LOGIC
# ══════════════════════════════════════════════════════════════════════════════

def generate_credentials():
    """Generate random credentials"""
    username = fake.user_name()
    email = f"{username}_{random.randint(100,999)}@{fake.free_email_domain()}"
    password = ''.join(random.choices(string.ascii_letters + string.digits, k=12)) + "!Aa1"
    
    return {
        'username': username,
        'email': email,
        'password': password,
    }

async def extract_token(page: Page) -> Optional[str]:
    """Try to extract auth token from page/localStorage/cookies"""
    
    # Check localStorage
    try:
        storage = await page.evaluate('''() => {
            let tokens = {};
            for (let key in localStorage) {
                let val = localStorage[key];
                if (typeof val === 'string' && 
                    (val.includes('eyJ') || key.toLowerCase().includes('token') || 
                     key.toLowerCase().includes('auth'))) {
                    tokens[key] = val;
                }
            }
            return tokens;
        }''')
        
        if storage:
            # Return first token-like value
            for key, val in storage.items():
                if 'token' in key.lower() or val.startswith('eyJ'):
                    return val
    except:
        pass
    
    # Check cookies
    try:
        cookies = await page.context.cookies()
        for cookie in cookies:
            if 'token' in cookie['name'].lower() or 'auth' in cookie['name'].lower():
                return cookie['value']
    except:
        pass
    
    return None

async def auto_login_after_register(login_url: str, username_or_email: str, password: str, proxy: Optional[str] = None, visible: bool = False) -> Tuple[bool, Optional[str]]:
    """Auto login after successful registration using the credentials just created. Returns (success, token)."""
    
    if not login_url:
        print("[!] No login URL provided, skipping auto-login")
        return False, None
    
    print(f"\n[AUTO-LOGIN] Logging in with new credentials...")
    print(f"    Login URL: {login_url}")
    print(f"    Username: {username_or_email}")
    
    proxy_config = {"server": proxy} if proxy else None
    
    try:
        async with async_playwright() as pw:
            browser = await pw.chromium.launch(
                headless=not visible,
                proxy=proxy_config,
                args=["--no-sandbox", "--disable-dev-shm-usage"]
            )
            
            context = await browser.new_context(
                user_agent="Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
            )
            page = await context.new_page()
            
            # Navigate to login page
            print(f"    → Navigating to login page...")
            await _wait_with_modal_timeout(
                page.goto(login_url, timeout=30000),
                timeout=10,
                page=page,
                retry=True
            )
            await asyncio.sleep(2)
            
            # Try to close any initial modal
            await _try_close_modal_global(page)
            
            # Find and fill username/email field
            username_patterns = [
                'input[name="username"]',
                'input[name="email"]',
                'input[type="email"]',
                'input[id*="username"]',
                'input[id*="email"]',
                '[class*="username"] input',
                '[class*="email"] input',
            ]
            
            username_field = None
            for pattern in username_patterns:
                try:
                    elements = await page.query_selector_all(pattern)
                    for el in elements:
                        if await el.is_visible():
                            username_field = el
                            break
                    if username_field:
                        break
                except:
                    continue
            
            if username_field:
                await _wait_with_modal_timeout(
                    username_field.fill(username_or_email),
                    timeout=5,
                    page=page,
                    retry=False
                )
                print(f"    ✓ Username/Email filled")
            else:
                print(f"    ! Could not find username field")
                await browser.close()
                return False, None
            
            # Find and fill password field
            password_patterns = [
                'input[name="password"]',
                'input[type="password"]',
                'input[id*="password"]',
                '[class*="password"] input',
            ]
            
            password_field = None
            for pattern in password_patterns:
                try:
                    elements = await page.query_selector_all(pattern)
                    for el in elements:
                        if await el.is_visible():
                            password_field = el
                            break
                    if password_field:
                        break
                except:
                    continue
            
            if password_field:
                await _wait_with_modal_timeout(
                    password_field.fill(password),
                    timeout=5,
                    page=page,
                    retry=False
                )
                print(f"    ✓ Password filled")
            else:
                print(f"    ! Could not find password field")
                await browser.close()
                return False, None
            
            # Find and click submit button
            submit_patterns = [
                "button[type='submit']",
                "input[type='submit']",
                "button:has-text('Login')",
                "button:has-text('login')",
                "button:has-text('Sign In')",
                "button:has-text('Đăng nhập')",
                "button[class*='submit']",
                "button[class*='login']",
            ]
            
            submitted = False
            for pattern in submit_patterns:
                try:
                    btn = await page.query_selector(pattern)
                    if btn and await btn.is_visible():
                        await _wait_with_modal_timeout(
                            btn.click(timeout=3000),
                            timeout=5,
                            page=page,
                            retry=False
                        )
                        submitted = True
                        print(f"    ✓ Login form submitted")
                        break
                except:
                    continue
            
            if not submitted:
                # Enter key disabled - too many false positives
                print(f"    ! Could not find submit button")
                await browser.close()
                return False, None
            
            # Wait for response
            await asyncio.sleep(5)
            
            # Check if login was successful
            page_content = (await page.content()).lower()
            page_url = page.url.lower()
            
            success_indicators = [
                "welcome", "dashboard", "home", "profile",
                "logout", "đã đăng nhập", "chào mừng",
                "welcome back", "success"
            ]
            
            is_success = any(ind in page_content or ind in page_url for ind in success_indicators)
            
            # Try to extract token
            token = await extract_token(page)
            
            await browser.close()
            
            if is_success:
                print(f"    ✓ AUTO-LOGIN SUCCESSFUL")
                if token:
                    print(f"    ✓ Token extracted: {token[:50]}...")
                return True, token
            else:
                print(f"    ? Login status unclear")
                return False, None
    
    except Exception as e:
        print(f"    ! Login error: {str(e)[:100]}")
        return False, None

async def auto_register(url: str, proxy: Optional[str] = None, visible: bool = False, register_url: Optional[str] = None, login_url: Optional[str] = None):
    """Smart auto registration with intelligent form filling + auto-login"""
    
    proxy_config = {"server": proxy} if proxy else None
    creds = generate_credentials()
    
    print("")
    print("="*70)
    print("           SMART AUTO REGISTER - INTELLIGENT FORM FILLING")
    print("="*70)
    print(f"Target: {url}")
    if register_url:
        print(f"Register URL: {register_url}")
    print("")
    
    async with async_playwright() as pw:
        browser = await pw.chromium.launch(
            headless=not visible,
            proxy=proxy_config,
            args=["--no-sandbox", "--disable-dev-shm-usage"]
        )
        
        context = await browser.new_context(
            user_agent="Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
        )
        page = await context.new_page()
        
        # ═══ Navigate to register endpoint ═══
        if register_url:
            # Use provided register URL directly
            print(f"[1/5] Navigating to register endpoint: {register_url}")
            await _wait_with_modal_timeout(
                page.goto(register_url, timeout=30000),
                timeout=10,
                page=page,
                retry=True
            )
            await asyncio.sleep(2)
            print("    ✓ Register form loaded")
        else:
            # Find register button from homepage
            print(f"[1/5] Opening target...")
            await _wait_with_modal_timeout(
                page.goto(url, timeout=30000),
                timeout=10,
                page=page,
                retry=True
            )
            await asyncio.sleep(2)
            
            # Try to close any initial modal (welcome, help, etc.)
            await _try_close_modal_global(page)
            
            # ═══ Find register button using REGEX ═══
            print(f"[2/5] Finding register button (regex-based)...")
            finder = RegisterPageFinder(url)
            register_elements = await finder._find_register_elements_by_regex(page)
            
            clicked = False
            base_url = url.rstrip("/").split('#')[0].rstrip('/')
            
            # Try visible elements first
            for el, match_info, is_visible in register_elements:
                if is_visible:
                    try:
                        # Check if element has href for SPA navigation
                        el_href = await el.get_attribute("href")
                        if el_href:
                            # Resolve fragment-based URLs for SPA
                            if el_href.startswith("#"):
                                target_url = base_url + el_href
                            else:
                                target_url = el_href if el_href.startswith("http") else base_url + el_href
                            
                            text = await el.inner_text()
                            print(f"    ✓ Found: {text.strip()} → {match_info}")
                            print(f"    ↳ Navigating to: {target_url}")
                            await _wait_with_modal_timeout(
                                page.goto(target_url, timeout=30000),
                                timeout=10,
                                page=page,
                                retry=True
                            )
                            await asyncio.sleep(2)
                            clicked = True
                            break
                        else:
                            # No href, just click
                            text = await el.inner_text()
                            print(f"    ✓ Found: {text.strip()} → {match_info}")
                            await _wait_with_modal_timeout(
                                el.click(force=True),
                                timeout=5,
                                page=page,
                                retry=False
                            )
                            await asyncio.sleep(2)
                            clicked = True
                            break
                    except Exception as e:
                        continue
            
            # Try non-visible elements with force click
            if not clicked:
                for el, match_info, is_visible in register_elements:
                    if not is_visible:
                        try:
                            el_href = await el.get_attribute("href")
                            if el_href:
                                if el_href.startswith("#"):
                                    target_url = base_url + el_href
                                else:
                                    target_url = el_href if el_href.startswith("http") else base_url + el_href
                                
                                text = await el.inner_text()
                                print(f"    ↳ Register element (hidden): {text.strip()} → {match_info}")
                                await _wait_with_modal_timeout(
                                    page.goto(target_url, timeout=30000),
                                    timeout=10,
                                    page=page,
                                    retry=True
                                )
                                await asyncio.sleep(2)
                                clicked = True
                                break
                            else:
                                await _wait_with_modal_timeout(
                                    el.click(force=True),
                                    timeout=5,
                                    page=page,
                                    retry=False
                                )
                                await asyncio.sleep(2)
                                clicked = True
                                break
                        except:
                            continue
            
            if not clicked:
                print("    ! No register button found via regex, trying direct form fill")
        
        # Try to close any modal before filling form
        await _try_close_modal_global(page)
        
        # ═══ Fill form intelligently ═══
        step_prefix = "[2/4]" if register_url else "[3/5]"
        print(f"{step_prefix} Filling registration form...")
        filler = SmartFormFiller(creds)
        await filler.fill_form(page)
        
        # ═══ Submit ═══
        step_prefix = "[3/4]" if register_url else "[4/5]"
        print(f"{step_prefix} Submitting form...")
        
        submit_selectors = [
            "button[type='submit']",
            "input[type='submit']",
            "button:has-text('Đăng ký')", "button:has-text('Register')",
            "button:has-text('Sign up')", "button:has-text('Tạo tài khoản')",
            "button:has-text('Submit')", "button:has-text('Gửi')",
        ]
        
        submitted = False
        
        # Try to close any modal/dialog before submission (using global watchdog)
        try:
            for sel in submit_selectors:
                try:
                    # Wrap submit click with timeout protection
                    await _wait_with_modal_timeout(
                        page.click(sel, timeout=3000),
                        timeout=5,
                        page=page,
                        retry=False
                    )
                    submitted = True
                    print(f"    ✓ Form submitted")
                    break
                except:
                    continue
            
            if not submitted:
                print("    ! Could not find submit button")
        except:
            pass
        
        # Wait for response
        await asyncio.sleep(5)
        
        # ═══ Check success & extract token ═══
        step_prefix = "[4/4]" if register_url else "[5/5]"
        print(f"{step_prefix} Checking result...")
        
        success_indicators = [
            "welcome", "success", "successfully", "verify", "check your email",
            "thành công", "xác nhận", "kiểm tra email", "chúc mừng",
            "congratulations", "account created", "registration complete"
        ]
        
        page_content = (await page.content()).lower()
        page_url = page.url.lower()
        
        is_success = any(ind in page_content or ind in page_url for ind in success_indicators)
        
        # Extract token
        token = await extract_token(page)
        
        await browser.close()
        
        # ═══ Print results ═══
        print("")
        print("="*70)
        if is_success:
            print("                    ✓ REGISTRATION SUCCESSFUL")
        else:
            print("                    ? REGISTRATION STATUS UNKNOWN")
        print("="*70)
        print("")
        print("📧 EMAIL:    ", creds['email'])
        print("👤 USERNAME: ", creds['username'])
        print("🔑 PASSWORD: ", creds['password'])
        
        if token:
            print("🎟️  TOKEN:    ", token[:80] + "..." if len(token) > 80 else token)
        else:
            print("🎟️  TOKEN:     (not found)")
        
        print("")
        print("="*70)
        
        # Save to file
        output = {
            'timestamp': datetime.now().isoformat(),
            'url': url,
            'credentials': creds,
            'token': token,
            'status': 'success' if is_success else 'unknown',
            'filled_fields': filler.filled_fields
        }
        
        # Save session format tương thích với deep_crawl.sh
        session_data = {
            'timestamp': datetime.now().isoformat(),
            'cookies': {'token': token} if token else {},
            'headers': {
                'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36',
                'Authorization': f'Bearer {token}' if token else ''
            },
            'cookie_string': f'token={token}' if token else '',
            'base_url': url
        }
        
        with open('registered_accounts.json', 'a') as f:
            f.write(json.dumps(output, indent=2, ensure_ascii=False) + '\n')
        
        with open('session_cookies.json', 'w') as f:
            json.dump(session_data, f, indent=2, ensure_ascii=False)
        
        print("[*] Saved to: registered_accounts.json")
        print("[*] Session saved to: session_cookies.json (for deep_crawl.sh)")
        print("")
        
        # If registration was successful, try auto-login
        if is_success and login_url:
            print("\n" + "="*70)
            # Try email first, fallback to username
            login_username = creds['email']
            login_success, login_token = await auto_login_after_register(login_url, login_username, creds['password'], proxy, visible)
            if not login_success:
                # Try with username instead
                login_username = creds['username']
                login_success, login_token = await auto_login_after_register(login_url, login_username, creds['password'], proxy, visible)
            
            # If auto-login succeeded, save the token to session_cookies.json
            if login_success and login_token:
                print(f"\n[*] Saving auto-login session to session_cookies.json...")
                auto_login_session = {
                    'timestamp': datetime.now().isoformat(),
                    'cookies': {'token': login_token},
                    'headers': {
                        'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36',
                        'Authorization': f'Bearer {login_token}'
                    },
                    'cookie_string': f'token={login_token}',
                    'base_url': url,
                    'credentials': {
                        'username': creds['username'],
                        'email': creds['email']
                    }
                }
                with open('session_cookies.json', 'w') as f:
                    json.dump(auto_login_session, f, indent=2, ensure_ascii=False)
                print(f"[✓] Auto-login session saved: token={login_token[:50]}...")
            print("="*70)
        
        return output if is_success else None


async def main():
    parser = argparse.ArgumentParser(
        description="Smart Auto Register - Intelligent Form Filling + Auto-Login"
    )
    parser.add_argument("-u", "--url", required=True, help="Target URL")
    parser.add_argument("--proxy", help="Proxy (e.g. http://127.0.0.1:8080)")
    parser.add_argument("--register-url", help="Direct register endpoint URL (skip detection)")
    parser.add_argument("--login-url", help="Login endpoint URL (for auto-login after register)")
    parser.add_argument("--visible", action="store_true", help="Show browser")
    parser.add_argument("-n", "--count", type=int, default=1, help="Number of accounts")
    args = parser.parse_args()
    
    success_count = 0
    
    for i in range(args.count):
        if args.count > 1:
            print(f"\n{'='*70}")
            print(f"  REGISTRATION {i+1}/{args.count}")
            print(f"{'='*70}")
        
        result = await auto_register(args.url, args.proxy, args.visible, args.register_url, args.login_url)
        
        if result:
            success_count += 1
        
        # Delay between registrations
        if i < args.count - 1:
            delay = random.uniform(10, 20)
            print(f"[*] Waiting {delay:.1f}s before next registration...")
            await asyncio.sleep(delay)
    
    if args.count > 1:
        print("")
        print("="*70)
        print(f"       BATCH COMPLETE: {success_count}/{args.count} successful")
        print("="*70)

if __name__ == "__main__":
    asyncio.run(main())
