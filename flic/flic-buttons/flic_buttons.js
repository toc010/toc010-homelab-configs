const buttonManager = require("buttons");
const http = require("http");

// ════════════════════════════════════════════════
//  CONFIGURE — 4 values to fill in
// ════════════════════════════════════════════════

const HA_URL   = "http://X.X.X.X:8123";      // your HA local IP + port
const HA_TOKEN = "YOUR_HA_TOKEN";  // HA profile → Long-Lived Access Tokens

// Lutron entity IDs — HA Developer Tools → States → filter light.
const ROOM1_ROOM_LIGHT = "switch.room1_bedroom_lamps";      // adjust to match exactly
const ROOM2_ROOM_LIGHT = "switch.room2_bedroom_lamps";      // adjust to match exactly

// Pulsing indicator lights (real `light.` entities, support rgb_color)
const BASEMENT_LAMP = "light.basement_game_area_basement_table_lamp";
const LIVING_ROOM_LIGHTS = [
  "light.family_room_living_room_table_lamp_left",
  "light.family_room_living_room_table_lamp_right",
  "light.wiz_rgbw_tunable_XXXXX"
];

// ════════════════════════════════════════════════
//  BUTTON ADDRESSES — pre-filled, do not change
// ════════════════════════════════════════════════

const BTN_PHONE1     = "XX:XX:XX:XX:XX:XX";  // XXXX-XXXXXX → Phone 1
const BTN_PHONE2     = "XX:XX:XX:XX:XX:XX";  // XXXX-XXXXXX → Phone 2
const BTN_ROOM1_ROOM = "XX:XX:XX:XX:XX:XX";  // XXXX-XXXXXX → Room1 Room
const BTN_ROOM2_ROOM = "XX:XX:XX:XX:XX:XX";  // XXXX-XXXXXX → Room2 Room

// ════════════════════════════════════════════════
//  NTFY TOPICS — one per phone
//  Must match the topic subscribed to in the ntfy app
// ════════════════════════════════════════════════

const NTFY_PHONE1 = "home_phone1_locate";
const NTFY_PHONE2 = "home_phone2_locate";

const NOTIFY_PHONE1 = "mobile_phone1";
const NOTIFY_PHONE2 = "mobile_phone2"; 

// Pulse colors
const COLOR_GREEN = [0, 255, 0];
const COLOR_RED   = [255, 0, 0];

// ════════════════════════════════════════════════
//  HELPERS — notifications / switches
// ════════════════════════════════════════════════

function ringPhone(notifyService, label) {
  http.makeRequest({
    url: HA_URL + "/api/services/notify/" + notifyService,
    method: "POST",
    headers: {
      "Authorization": "Bearer " + HA_TOKEN,
      "Content-Type": "application/json"
    },
    content: JSON.stringify({
      message: label + " is being looked for",
      title: "Find my phone",
      data: {
        push: {
          sound: {
            name: "default",
            critical: 1,
            volume: 1.0
          }
        }
      }
    })
  }, function(err, res) {
    if (err) console.log("[ERROR] notify", label, err);
    else console.log("[OK] notify", label, res.statusCode);
  });
}

function switchOn(entityId, label) {
  http.makeRequest({
    url: HA_URL + "/api/services/switch/turn_on",
    method: "POST",
    headers: {
      "Authorization": "Bearer " + HA_TOKEN,
      "Content-Type": "application/json"
    },
    content: JSON.stringify({ entity_id: entityId })
  }, function(err, res) {
    if (err) console.log("[ERROR] turn_on", label, err);
    else console.log("[OK] turn_on", label, res.statusCode);
  });
}

function switchOff(entityId, label) {
  http.makeRequest({
    url: HA_URL + "/api/services/switch/turn_off",
    method: "POST",
    headers: {
      "Authorization": "Bearer " + HA_TOKEN,
      "Content-Type": "application/json"
    },
    content: JSON.stringify({ entity_id: entityId })
  }, function(err, res) {
    if (err) console.log("[ERROR] turn_off", label, err);
    else console.log("[OK] turn_off", label, res.statusCode);
  });
}

// ════════════════════════════════════════════════
//  HELPERS — pulsing light indicators
// ════════════════════════════════════════════════

function lightColorOn(entityId, rgbColor, label) {
  http.makeRequest({
    url: HA_URL + "/api/services/light/turn_on",
    method: "POST",
    headers: {
      "Authorization": "Bearer " + HA_TOKEN,
      "Content-Type": "application/json"
    },
    content: JSON.stringify({
      entity_id: entityId,
      rgb_color: rgbColor,
      brightness: 255
    })
  }, function(err, res) {
    if (err) console.log("[ERROR] light on", label, err);
    else console.log("[OK] light on", label, res.statusCode);
  });
}

function lightOff(entityId, label) {
  http.makeRequest({
    url: HA_URL + "/api/services/light/turn_off",
    method: "POST",
    headers: {
      "Authorization": "Bearer " + HA_TOKEN,
      "Content-Type": "application/json"
    },
    content: JSON.stringify({ entity_id: entityId })
  }, function(err, res) {
    if (err) console.log("[ERROR] light off", label, err);
    else console.log("[OK] light off", label, res.statusCode);
  });
}

// Track active pulse timers per lamp group so a new trigger cancels the old one
// priorStates holds each entity's state/attributes as captured right before a pulse began,
// so the group can be restored to exactly what it was after the pulse ends.
const pulseState = {
  basement:   { interval: null, timeout: null, priorStates: null },
  livingroom: { interval: null, timeout: null, priorStates: null }
};

// Fetch current state for a list of entity_ids from HA.
// callback(results) where results = { entityId: { state, attributes, ... } }
function fetchStates(entityIds, callback) {
  const results = {};
  let remaining = entityIds.length;

  if (remaining === 0) {
    callback(results);
    return;
  }

  entityIds.forEach(function(entityId) {
    http.makeRequest({
      url: HA_URL + "/api/states/" + entityId,
      method: "GET",
      headers: {
        "Authorization": "Bearer " + HA_TOKEN
      }
    }, function(err, res) {
      if (!err && res && res.data) {
        try {
          results[entityId] = JSON.parse(res.data);
        } catch (e) {
          console.log("[ERROR] parse state", entityId, e);
        }
      } else {
        console.log("[ERROR] fetch state", entityId, err);
      }
      remaining--;
      if (remaining === 0) callback(results);
    });
  });
}

// Restore each light in entityIds to whatever it was doing before the pulse started.
function restoreLights(entityIds, priorStates, label) {
  entityIds.forEach(function(entityId) {
    const prior = priorStates && priorStates[entityId];

    if (!prior) {
      // We never captured a prior state (fetch failed) — safest fallback is off.
      lightOff(entityId, label);
      return;
    }

    if (prior.state === "on") {
      const attrs = prior.attributes || {};
      const payload = { entity_id: entityId };
      if (attrs.rgb_color) payload.rgb_color = attrs.rgb_color;
      if (attrs.brightness !== undefined) payload.brightness = attrs.brightness;
      if (attrs.color_temp) payload.color_temp = attrs.color_temp;

      http.makeRequest({
        url: HA_URL + "/api/services/light/turn_on",
        method: "POST",
        headers: {
          "Authorization": "Bearer " + HA_TOKEN,
          "Content-Type": "application/json"
        },
        content: JSON.stringify(payload)
      }, function(err, res) {
        if (err) console.log("[ERROR] restore on", label, entityId, err);
        else console.log("[OK] restored on", label, entityId, res.statusCode);
      });
    } else {
      lightOff(entityId, label);
    }
  });
}

// entityIds: array of light entity_ids to pulse together
// rgbColor: [r,g,b]
// durationMs: total pulse duration
// intervalMs: how often the lights toggle on/off during the pulse
// groupKey: "basement" or "livingroom" — used to track/cancel overlapping runs
// label: for logging
function pulseLights(entityIds, rgbColor, durationMs, intervalMs, groupKey, label) {
  const state = pulseState[groupKey];

  // Cancel any pulse already running on this group
  if (state.interval) clearInterval(state.interval);
  if (state.timeout) clearTimeout(state.timeout);

  // Capture current state first, THEN start pulsing, so restore has something to go back to
  fetchStates(entityIds, function(priorStates) {
    state.priorStates = priorStates;

    let isOn = false;

    state.interval = setInterval(function() {
      isOn = !isOn;
      entityIds.forEach(function(entityId) {
        if (isOn) {
          lightColorOn(entityId, rgbColor, label);
        } else {
          lightOff(entityId, label);
        }
      });
    }, intervalMs);

    state.timeout = setTimeout(function() {
      clearInterval(state.interval);
      state.interval = null;
      state.timeout = null;
      restoreLights(entityIds, state.priorStates, label);
      state.priorStates = null;
      console.log("[DONE] pulse ended for", label, "— restored prior state");
    }, durationMs);

    console.log("[START] pulse", label, "for", durationMs / 1000, "s, toggling every", intervalMs / 1000, "s");
  });
}

// ════════════════════════════════════════════════
//  BUTTON EVENTS
// ════════════════════════════════════════════════

buttonManager.on("buttonSingleOrDoubleClickOrHold", function(obj) {

  switch (obj.bdaddr) {

    case BTN_PHONE1:
      if (obj.isSingleClick) {
        ringPhone(NOTIFY_PHONE1, "Phone 1");
        setTimeout(function() { ringPhone(NOTIFY_PHONE1, "Phone 1"); }, 5000);
        setTimeout(function() { ringPhone(NOTIFY_PHONE1, "Phone 1"); }, 10000);
        setTimeout(function() { ringPhone(NOTIFY_PHONE1, "Phone 1"); }, 15000);
        setTimeout(function() { ringPhone(NOTIFY_PHONE1, "Phone 1"); }, 20000);
      }
      if (obj.isDoubleClick) {
        pulseLights([BASEMENT_LAMP], COLOR_GREEN, 30000, 5000, "basement", "Basement Lamp (green)");
      }
      if (obj.isHold) {
        pulseLights([BASEMENT_LAMP], COLOR_RED, 60000, 5000, "basement", "Basement Lamp (red)");
      }
      break;

    case BTN_PHONE2:
      if (obj.isSingleClick) {
        ringPhone(NOTIFY_PHONE2, "Phone 2");
        setTimeout(function() { ringPhone(NOTIFY_PHONE2, "Phone 2"); }, 5000);
        setTimeout(function() { ringPhone(NOTIFY_PHONE2, "Phone 2"); }, 10000);
        setTimeout(function() { ringPhone(NOTIFY_PHONE2, "Phone 2"); }, 15000);
        setTimeout(function() { ringPhone(NOTIFY_PHONE2, "Phone 2"); }, 20000);
      }
      if (obj.isDoubleClick) {
        pulseLights(LIVING_ROOM_LIGHTS, COLOR_GREEN, 30000, 5000, "livingroom", "Living Room Lamps (green)");
      }
      if (obj.isHold) {
        pulseLights(LIVING_ROOM_LIGHTS, COLOR_RED, 60000, 5000, "livingroom", "Living Room Lamps (red)");
      }
      break;

    case BTN_ROOM1_ROOM:
      if (obj.isSingleClick)  switchOn(ROOM1_ROOM_LIGHT,  "Room1 room");
      if (obj.isDoubleClick)  switchOff(ROOM1_ROOM_LIGHT, "Room1 room");
      break;

    case BTN_ROOM2_ROOM:
      if (obj.isSingleClick)  switchOn(ROOM2_ROOM_LIGHT,  "Room2 room");
      if (obj.isDoubleClick)  switchOff(ROOM2_ROOM_LIGHT, "Room2 room");
      break;

    default:
      console.log("[UNKNOWN]", obj.bdaddr);
  }
});

console.log("Flic → ntfy + HA ready.");
