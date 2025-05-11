<?php
$mac = '0010A4123456'; // Replace with actual MAC (no colons)
$topic = "splashpad/$mac/controller/status";

if (isset($_GET['ajax'])) {
    $raw = shell_exec("mosquitto_sub -h [mqtthost.domain.tld] -t '$topic' -C 1 -u [USERNAME] -P [PASSWORD] -W 2");
    $control = json_decode($raw, true);

    if (!is_array($control)) {
        $control = ['temperature' => 0, 'status' => 'Unknown'];
    }

    header('Content-Type: application/json');

    $temperature = $control['temperature'] ?? 0;
    $on_temp     = $control['on_temp'] ?? 74;
    $maintenance = $control['maintenance'] ?? false;
    $icon = '⛔';
    $status = 'Off';
    $message = 'Splash pad is currently off.';

    $now = new DateTime("now", new DateTimeZone("America/Los_Angeles"));
    $current_hour = (int)$now->format("G");
    $today = $now->format("Y-m-d");
    $season_start = $now->format("Y") . '-05-11';

    if ($maintenance) {
        $icon = '🛠️';
        $message = 'Splash pad is in maintenance mode.';
        $status = 'Maintenance';
    } elseif (($control['status'] ?? '') == "ON" && $today >= $season_start && $temperature >= $on_temp && $current_hour >= 10 && $current_hour < 20) {
        $status = 'On';
        $icon = '💧';
        $message = 'Splash pad is running!';
    } elseif ($today < $season_start) {
        $icon = '📅';
        $message = 'Season not started (starts May 15).';
    } elseif ($temperature < $on_temp) {
        $icon = '🌡️';
        $message = 'Waiting for ' . $on_temp . '°F+ to start.';
    } elseif ($current_hour < 1 || $current_hour >= 20) {
        $icon = '🌙';
        $message = 'Open daily from 10 AM – 8 PM.';
    }

    echo json_encode([
        'temperature' => $temperature,
        'status' => $status,
        'message' => $message,
        'icon' => $icon
    ]);
    exit;
}
?>
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Splash Pad Widget</title>
  <style>
    html, body {
      margin: 0;
      padding: 0;
      height: 100%;
      background: radial-gradient(circle at top left, #b2ebf2, #81d4fa, #4fc3f7);
      font-family: sans-serif;
      overflow: hidden;
    }

    #splash-widget {
      position: relative;
      width: 320px;
      height: 100px;
      margin: 10px auto;
      background: rgba(255,255,255,0.9);
      border-radius: 20px;
      box-shadow: 0 0 10px rgba(0,0,0,0.2);
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      text-align: center;
      z-index: 1;
      animation: fadein 1s ease-in;
    }

    @keyframes fadein {
      from { opacity: 0; transform: translateY(10px); }
      to { opacity: 1; transform: translateY(0); }
    }

    #splash-icon {
      font-size: 34px;
      margin-top: 10px;
      animation: pulse 2s infinite;
    }

    @keyframes pulse {
      0% { transform: scale(1); opacity: 1; }
      50% { transform: scale(1.1); opacity: 0.7; }
      100% { transform: scale(1); opacity: 1; }
    }

    #splash-status {
      font-weight: bold;
      font-size: 15px;
      text-align: center;
      width: 100%;
    }

    #splash-temp {
      font-size: 13px;
      color: #01579b;
      text-align: center;
      width: 100%;
    }

    #rain {
      position: absolute;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      pointer-events: none;
      z-index: 0;
    }

    .drop {
      position: absolute;
      bottom: 100%;
      width: 4px;
      height: 10px;
      background: radial-gradient(ellipse at center, #90caf9 0%, #42a5f5 100%);
      opacity: 0.8;
      border-radius: 50% 50% 70% 70% / 70% 70% 40% 40%;
      animation: drop 1.8s ease-in infinite;
      box-shadow: 0 0 3px rgba(33,150,243,0.6);
      transform: scaleY(1.05);
    }

    @keyframes drop {
      0% { transform: translateY(0); opacity: 0.8; }
      80% { opacity: 1; }
      100% { transform: translateY(100px); opacity: 0; }
    }
  </style>
  <script src="https://code.jquery.com/jquery-3.7.1.min.js"></script>
</head>
<body>
<div id="splash-widget">
  <div id="rain"></div>
  <div id="splash-icon">🔄</div>
  <div id="splash-status">Loading...</div>
  <div id="splash-temp"></div>
</div>

<script>
const rainContainer = document.getElementById('rain');
for (let i = 0; i < 12; i++) {
  const drop = document.createElement('div');
  drop.className = 'drop';
  drop.style.left = Math.random() * 100 + '%';
  drop.style.animationDelay = (Math.random() * 2) + 's';
  rainContainer.appendChild(drop);
}

function updateSplashStatus() {
  $.getJSON('?ajax=1', function(data) {
    $('#splash-icon').text(data.icon);
    $('#splash-status').text(data.message);
    $('#splash-temp').text('Outside Temperature: ' + data.temperature + '°F');
    rainContainer.style.display = (data.status === 'On') ? 'block' : 'none';
  });
}

updateSplashStatus();
setInterval(updateSplashStatus, 1000); // every 10s
</script>
</body>
</html>
