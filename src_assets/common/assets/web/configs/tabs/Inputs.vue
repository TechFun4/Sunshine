<script setup>
import { ref, computed } from 'vue'
import PlatformLayout from '../../PlatformLayout.vue'
import Checkbox from "../../Checkbox.vue";

const props = defineProps([
  'platform',
  'config'
])

const config = ref(props.config)

// Key mappings management
const newMappingFrom = ref('')
const newMappingTo = ref('')

// Parse keybindings from config format [from1, to1, from2, to2, ...]
const keyMappings = computed(() => {
  if (!config.value.keybindings) return []
  const bindings = config.value.keybindings
  // Handle both string and array formats
  let arr = bindings
  if (typeof bindings === 'string') {
    // Parse string format like "[0x41, 0x42, 0x43, 0x44]"
    const match = bindings.match(/\[([^\]]*)\]/)
    if (match) {
      arr = match[1].split(',').map(s => s.trim()).filter(s => s)
    } else {
      return []
    }
  }
  const mappings = []
  for (let i = 0; i < arr.length; i += 2) {
    if (i + 1 < arr.length) {
      mappings.push({ from: arr[i], to: arr[i + 1] })
    }
  }
  return mappings
})

function addKeyMapping() {
  if (!newMappingFrom.value || !newMappingTo.value) return

  // Parse the values (handle hex or decimal)
  const fromVal = newMappingFrom.value.trim()
  const toVal = newMappingTo.value.trim()

  // Get current mappings array
  let current = []
  if (config.value.keybindings) {
    if (typeof config.value.keybindings === 'string') {
      const match = config.value.keybindings.match(/\[([^\]]*)\]/)
      if (match) {
        current = match[1].split(',').map(s => s.trim()).filter(s => s)
      }
    } else {
      current = [...config.value.keybindings]
    }
  }

  // Add new mapping
  current.push(fromVal, toVal)

  // Update config
  config.value.keybindings = '[' + current.join(', ') + ']'

  // Clear inputs
  newMappingFrom.value = ''
  newMappingTo.value = ''
}

function removeKeyMapping(index) {
  let current = []
  if (config.value.keybindings) {
    if (typeof config.value.keybindings === 'string') {
      const match = config.value.keybindings.match(/\[([^\]]*)\]/)
      if (match) {
        current = match[1].split(',').map(s => s.trim()).filter(s => s)
      }
    } else {
      current = [...config.value.keybindings]
    }
  }

  // Remove the mapping (2 elements: from and to)
  current.splice(index * 2, 2)

  // Update config
  config.value.keybindings = current.length > 0 ? '[' + current.join(', ') + ']' : ''
}

// Common key codes for reference
const commonKeys = [
  { code: '0x41', name: 'A' }, { code: '0x42', name: 'B' }, { code: '0x43', name: 'C' },
  { code: '0x44', name: 'D' }, { code: '0x45', name: 'E' }, { code: '0x46', name: 'F' },
  { code: '0x47', name: 'G' }, { code: '0x48', name: 'H' }, { code: '0x49', name: 'I' },
  { code: '0x4A', name: 'J' }, { code: '0x4B', name: 'K' }, { code: '0x4C', name: 'L' },
  { code: '0x4D', name: 'M' }, { code: '0x4E', name: 'N' }, { code: '0x4F', name: 'O' },
  { code: '0x50', name: 'P' }, { code: '0x51', name: 'Q' }, { code: '0x52', name: 'R' },
  { code: '0x53', name: 'S' }, { code: '0x54', name: 'T' }, { code: '0x55', name: 'U' },
  { code: '0x56', name: 'V' }, { code: '0x57', name: 'W' }, { code: '0x58', name: 'X' },
  { code: '0x59', name: 'Y' }, { code: '0x5A', name: 'Z' },
  { code: '0x30', name: '0' }, { code: '0x31', name: '1' }, { code: '0x32', name: '2' },
  { code: '0x33', name: '3' }, { code: '0x34', name: '4' }, { code: '0x35', name: '5' },
  { code: '0x36', name: '6' }, { code: '0x37', name: '7' }, { code: '0x38', name: '8' },
  { code: '0x39', name: '9' },
  { code: '0x20', name: 'Space' }, { code: '0x0D', name: 'Enter' }, { code: '0x1B', name: 'Escape' },
  { code: '0x08', name: 'Backspace' }, { code: '0x09', name: 'Tab' },
  { code: '0x25', name: 'Left Arrow' }, { code: '0x26', name: 'Up Arrow' },
  { code: '0x27', name: 'Right Arrow' }, { code: '0x28', name: 'Down Arrow' },
  { code: '0x10', name: 'Shift' }, { code: '0x11', name: 'Ctrl' }, { code: '0x12', name: 'Alt' },
  { code: '0x5B', name: 'Windows' },
  { code: '0x70', name: 'F1' }, { code: '0x71', name: 'F2' }, { code: '0x72', name: 'F3' },
  { code: '0x73', name: 'F4' }, { code: '0x74', name: 'F5' }, { code: '0x75', name: 'F6' },
  { code: '0x76', name: 'F7' }, { code: '0x77', name: 'F8' }, { code: '0x78', name: 'F9' },
  { code: '0x79', name: 'F10' }, { code: '0x7A', name: 'F11' }, { code: '0x7B', name: 'F12' },
]

function getKeyName(code) {
  const key = commonKeys.find(k => k.code.toLowerCase() === String(code).toLowerCase())
  return key ? key.name : code
}
</script>

<template>
  <div id="input" class="config-page">
    <!-- Enable Gamepad Input -->
    <Checkbox class="mb-3"
              id="controller"
              locale-prefix="config"
              v-model="config.controller"
              default="true"
    ></Checkbox>

    <!-- Emulated Gamepad Type -->
    <div class="mb-3" v-if="config.controller === 'enabled' && platform !== 'macos'">
      <label for="gamepad" class="form-label">{{ $t('config.gamepad') }}</label>
      <select id="gamepad" class="form-select" v-model="config.gamepad">
        <option value="auto">{{ $t('_common.auto') }}</option>

        <PlatformLayout :platform="platform">
          <template #freebsd>
            <option value="switch">{{ $t("config.gamepad_switch") }}</option>
            <option value="xone">{{ $t("config.gamepad_xone") }}</option>
          </template>

          <template #linux>
            <option value="ds5">{{ $t("config.gamepad_ds5") }}</option>
            <option value="switch">{{ $t("config.gamepad_switch") }}</option>
            <option value="xone">{{ $t("config.gamepad_xone") }}</option>
          </template>

          <template #windows>
            <option value="ds4">{{ $t('config.gamepad_ds4') }}</option>
            <option value="x360">{{ $t('config.gamepad_x360') }}</option>
          </template>
        </PlatformLayout>
      </select>
      <div class="form-text">{{ $t('config.gamepad_desc') }}</div>
    </div>

    <!-- Additional options based on gamepad type -->
    <template v-if="config.controller === 'enabled'">
      <template v-if="config.gamepad === 'ds4' || config.gamepad === 'ds5' || (config.gamepad === 'auto' && platform !== 'macos')">
        <div class="mb-3 accordion">
          <div class="accordion-item">
            <h2 class="accordion-header">
              <button class="accordion-button" type="button" data-bs-toggle="collapse"
                      data-bs-target="#panelsStayOpen-collapseOne">
                {{ $t(config.gamepad === 'ds4' ? 'config.gamepad_ds4_manual' : (config.gamepad === 'ds5' ? 'config.gamepad_ds5_manual' : 'config.gamepad_auto')) }}
              </button>
            </h2>
            <div id="panelsStayOpen-collapseOne" class="accordion-collapse collapse show"
                 aria-labelledby="panelsStayOpen-headingOne">
              <div class="accordion-body">
                <!-- Automatic detection options (for Windows and Linux) -->
                <template v-if="config.gamepad === 'auto' && (platform === 'windows' || platform === 'linux')">
                  <!-- Gamepad with motion-capability as DS4(Windows)/DS5(Linux) -->
                  <Checkbox class="mb-3"
                            id="motion_as_ds4"
                            locale-prefix="config"
                            v-model="config.motion_as_ds4"
                            default="true"
                  ></Checkbox>
                  <!-- Gamepad with touch-capability as DS4(Windows)/DS5(Linux) -->
                  <Checkbox class="mb-3"
                            id="touchpad_as_ds4"
                            locale-prefix="config"
                            v-model="config.touchpad_as_ds4"
                            default="true"
                  ></Checkbox>
                </template>
                <!-- DS4 option: DS4 back button as touchpad click (on Automatic: Windows only) -->
                <template v-if="config.gamepad === 'ds4' || (config.gamepad === 'auto' && platform === 'windows')">
                  <Checkbox class="mb-3"
                            id="ds4_back_as_touchpad_click"
                            locale-prefix="config"
                            v-model="config.ds4_back_as_touchpad_click"
                            default="true"
                  ></Checkbox>
                </template>
                <!-- DS5 Option: Controller MAC randomization (on Automatic: Linux only) -->
                <template v-if="config.gamepad === 'ds5' || (config.gamepad === 'auto' && platform === 'linux')">
                  <Checkbox class="mb-3"
                            id="ds5_inputtino_randomize_mac"
                            locale-prefix="config"
                            v-model="config.ds5_inputtino_randomize_mac"
                            default="true"
                  ></Checkbox>
                </template>
              </div>
            </div>
          </div>
        </div>
      </template>
    </template>

    <!-- Home/Guide Button Emulation Timeout -->
    <div class="mb-3" v-if="config.controller === 'enabled'">
      <label for="back_button_timeout" class="form-label">{{ $t('config.back_button_timeout') }}</label>
      <input type="text" class="form-control" id="back_button_timeout" placeholder="-1"
             v-model="config.back_button_timeout" />
      <div class="form-text">{{ $t('config.back_button_timeout_desc') }}</div>
    </div>

    <!-- Enable Keyboard Input -->
    <hr>
    <Checkbox class="mb-3"
              id="keyboard"
              locale-prefix="config"
              v-model="config.keyboard"
              default="true"
    ></Checkbox>

    <!-- Key Repeat Delay-->
    <div class="mb-3" v-if="config.keyboard === 'enabled' && platform === 'windows'">
      <label for="key_repeat_delay" class="form-label">{{ $t('config.key_repeat_delay') }}</label>
      <input type="text" class="form-control" id="key_repeat_delay" placeholder="500"
             v-model="config.key_repeat_delay" />
      <div class="form-text">{{ $t('config.key_repeat_delay_desc') }}</div>
    </div>

    <!-- Key Repeat Frequency-->
    <div class="mb-3" v-if="config.keyboard === 'enabled' && platform === 'windows'">
      <label for="key_repeat_frequency" class="form-label">{{ $t('config.key_repeat_frequency') }}</label>
      <input type="text" class="form-control" id="key_repeat_frequency" placeholder="24.9"
             v-model="config.key_repeat_frequency" />
      <div class="form-text">{{ $t('config.key_repeat_frequency_desc') }}</div>
    </div>

    <!-- Always send scancodes -->
    <Checkbox v-if="config.keyboard === 'enabled' && platform === 'windows'"
              class="mb-3"
              id="always_send_scancodes"
              locale-prefix="config"
              v-model="config.always_send_scancodes"
              default="true"
    ></Checkbox>

    <!-- Mapping Key AltRight to Key Windows -->
    <Checkbox v-if="config.keyboard === 'enabled'"
              class="mb-3"
              id="key_rightalt_to_key_win"
              locale-prefix="config"
              v-model="config.key_rightalt_to_key_win"
              default="false"
    ></Checkbox>

    <!-- Allowed Keys -->
    <div class="mb-3" v-if="config.keyboard === 'enabled'">
      <label for="allowed_keys" class="form-label">{{ $t('config.allowed_keys') }}</label>
      <input type="text" class="form-control" id="allowed_keys"
             :placeholder="$t('config.allowed_keys_placeholder')"
             v-model="config.allowed_keys" />
      <div class="form-text">{{ $t('config.allowed_keys_desc') }}</div>
    </div>

    <!-- Key Mappings -->
    <div class="mb-3" v-if="config.keyboard === 'enabled'">
      <label class="form-label">{{ $t('config.key_mappings') }}</label>
      <div class="form-text mb-2">{{ $t('config.key_mappings_desc') }}</div>

      <!-- Existing mappings list -->
      <div v-if="keyMappings.length > 0" class="mb-3">
        <table class="table table-sm table-bordered">
          <thead>
            <tr>
              <th>{{ $t('config.key_mappings_from') }}</th>
              <th>{{ $t('config.key_mappings_to') }}</th>
              <th style="width: 60px;"></th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(mapping, index) in keyMappings" :key="index">
              <td>{{ getKeyName(mapping.from) }} ({{ mapping.from }})</td>
              <td>{{ getKeyName(mapping.to) }} ({{ mapping.to }})</td>
              <td>
                <button type="button" class="btn btn-danger btn-sm" @click="removeKeyMapping(index)">
                  &times;
                </button>
              </td>
            </tr>
          </tbody>
        </table>
      </div>

      <!-- Add new mapping -->
      <div class="row g-2 align-items-end">
        <div class="col-auto">
          <label for="newMappingFrom" class="form-label">{{ $t('config.key_mappings_from') }}</label>
          <select class="form-select" id="newMappingFrom" v-model="newMappingFrom">
            <option value="">{{ $t('config.key_mappings_select') }}</option>
            <option v-for="key in commonKeys" :key="key.code" :value="key.code">
              {{ key.name }} ({{ key.code }})
            </option>
          </select>
        </div>
        <div class="col-auto">
          <label for="newMappingTo" class="form-label">{{ $t('config.key_mappings_to') }}</label>
          <select class="form-select" id="newMappingTo" v-model="newMappingTo">
            <option value="">{{ $t('config.key_mappings_select') }}</option>
            <option v-for="key in commonKeys" :key="key.code" :value="key.code">
              {{ key.name }} ({{ key.code }})
            </option>
          </select>
        </div>
        <div class="col-auto">
          <button type="button" class="btn btn-primary" @click="addKeyMapping">
            {{ $t('config.add') }}
          </button>
        </div>
      </div>
    </div>

    <!-- Enable Mouse Input -->
    <hr>
    <Checkbox class="mb-3"
              id="mouse"
              locale-prefix="config"
              v-model="config.mouse"
              default="true"
    ></Checkbox>

    <!-- High resolution scrolling support -->
    <Checkbox v-if="config.mouse === 'enabled'"
              class="mb-3"
              id="high_resolution_scrolling"
              locale-prefix="config"
              v-model="config.high_resolution_scrolling"
              default="true"
    ></Checkbox>

    <!-- Native pen/touch support -->
    <Checkbox v-if="config.mouse === 'enabled'"
              class="mb-3"
              id="native_pen_touch"
              locale-prefix="config"
              v-model="config.native_pen_touch"
              default="true"
    ></Checkbox>
  </div>
</template>

<style scoped>

</style>
