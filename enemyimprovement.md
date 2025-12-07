# Enemy Redesign Plan: "The Living Tiles"

## 1. General Design Code

- Make use of **States** inside the enemy’s own class.

---

## 2. Charging Enemy (The Tank)

**Current Issue:**  
> Charges blindly at the player's old position, easy to sidestep.

**Improved Mechanics: "The Homing Bull"**

### Prediction Logic (Interception Algorithm)

- **Goal:** Calculate the exact point where the Enemy and Player will collide if both keep moving at their current speeds.
- **The Math:** Solve for time $t$ in the equation:

  $$
  |\text{DistToPlayer} + \text{PlayerVel} \cdot t| = \text{ChargeSpeed} \cdot t
  $$

  This forms a Quadratic Equation:

  $$
  (\text{PlayerSpeed}^2 - \text{ChargeSpeed}^2)t^2 + 2(\text{Dist} \cdot \text{PlayerVel})t + \text{Dist}^2 = 0
  $$

  Solve for the smallest positive $t$.

- **The Target:**
  ```cpp
  PredictedPos = PlayerPos + (PlayerVel * t);
  ```
- **The Adjuster (Multiplier):** Apply a multiplier to the prediction vector to fine-tune difficulty.
  ```cpp
  FinalTarget = PlayerPos + (PlayerVel * t * predictionMultiplier);
  ```
  - `multiplier = 1.0`: Perfect interception (Player must dodge).
  - `multiplier > 1.0` (e.g., 1.2): Overshoot. Punishes players running away in a straight line, but easier to dodge by cutting inward.
  - `multiplier < 1.0` (e.g., 0.8): Undershoot. Catches players who try to juke backwards too early.

**Visuals**
- The Charge: A trail of white dust particles kicks up from the ground behind it.

---

## 3. Shooter Enemy (The Sniper)

**Current Issue:**  
> Projectiles are slow and predictable. Non-threatening.

**Improved Mechanics: "The Tri-Shot Sentinel"**

### Pattern: Shotgun Arc

- Instead of 1 bullet, fire **3 bullets simultaneously** in a narrow 15-degree cone. (Increase bullet speed for shotgun)
- (General logic for this already exists)
- This makes it much harder to "thread the needle" and dodge by standing still or moving slightly.

### Pattern: Rapid Fire

- Every 3rd attack, enters **"Barrage Mode"**: Fires 5 shots in quick succession at the player.
- Since the shots differ in time, each of the five bullets should not be fired at the same direction, but updates the player position for each bullet. (Also increase bullet speed, but slower than shotgun)

### AI Behavior

- **Kiting:** If the player gets within 15 units, the Shooter retreats while keeping firing behavior.
- **Decide which pattern:**
  - There’s a cooldown timer for every attack (5 seconds). When the timer is off, it would choose:
    - When player within 15 units: **70% shotgun, 30% rapid**
    - When outside 15 units: **70% rapid, 30% shotgun**

**Visuals**
- **Laser Sight:** A thin red line (`DrawLine3D`) locks onto the player before firing.
- **Muzzle Flash:** A burst of yellow particles at the firing point to visualize a gun fired (the particles would create a cone-like shape).

---

## 4. Support Enemy (The Buffer)

**Current Issue:**  
> Buffs are invisible. Player ignores them. Heals ineffective.

**Improved Mechanics: "The Tether"**

### Visible Link

- The Support shoots a **Beam (Tether)** connecting itself to the nearest non-support enemy.
- **Effect:** The tethered ally becomes **50% immune to damage** and **70% reduced knock back**.
- **Counterplay:** The player MUST kill the Support to break the shield.

### Healing Burst

- Instead of a slow tick, the Support pulses a **"Heal Wave"** (looks like a shock wave from vanguard but green color) every 10 seconds. Radius: 20 units.
- **Logic:** Finds the lowest HP ally in range (excludes self/other supports). Instantly restores 50% HP.

**Visuals**
- **The Tether:** A thick beam of yellow (long cube) connecting Support to Target.
- **The Pulse:** When healing, a green ring expands from the Support (use particle system and look at vanguard).
- **Feedback:** If the player shoots the linked Target, the damage indicator would be metallic gray.

---

## 5. Summoner Enemy (The Tactician)

**Current Issue:**  
> Just runs away slowly. Boring.

**Improved Mechanics: "The Blink"**

### Panic Teleport

- **Trigger:** If Player Distance < 5.0f.
- **Action:** Vanishes after 2 seconds of shaking (charging the teleportation) and reappears at the most far place from the player in the room(shouldn't leave the room or collide with the wall, make it a 2 unit margin from walls).
- **Cooldown:** 6 seconds.

**Visuals**
- **Teleport Out:** The Summoner implodes into a cloud of purple smoke particles.
- **Teleport In:** The Summoner comes out of a cloud of purple smoke particles.
- **Summon Minion:** Animation already exists. Don’t touch.

---

## Status
- [x] General Design: States used.
- [x] Charging Enemy: Prediction (Homing Bull), Dust Trails.
- [x] Shooter Enemy: Tri-Shot, Rapid Fire, Kiting, Laser Sight, Muzzle Flash.
- [x] Support Enemy: Tether (Visible Beam, Resistance), Healing Burst (Green Ring).
- [x] Summoner Enemy: Panic Teleport, Visuals.

the charging enemy prediction logic. please make it so that the prediction accounts for the time and distance. so if the player move without dodging, they would meet at the same point at the same time. account for the charging speed and player speed.  also provide a multiplier at the end to adjust to make it overshot or under shot the distance of player movement 
the supports link: make the linked enemy have yellow particles surround it.