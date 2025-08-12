import pygame
import random
import sys
import os
import math

# Initialize pygame
pygame.init()
pygame.mixer.init()

# Screen setup
WIDTH, HEIGHT = 900, 600
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("⚔️ Turn-Based Battle Game")

# Load assets (fallback to colored rectangles if images not found)
BASE_PATH = os.path.join(os.path.dirname(__file__), "assets")

PLAYER1_IMG = pygame.image.load(os.path.join(BASE_PATH, "P1_Shot.png"))
PLAYER2_IMG = pygame.image.load(os.path.join(BASE_PATH, "P2_Shot.png"))
PLAYER1_IMG = pygame.transform.scale(PLAYER1_IMG, (150, 150))
PLAYER2_IMG = pygame.transform.scale(PLAYER2_IMG, (150, 150))

# Enhanced Colors
WHITE = (255, 255, 255)
RED = (220, 50, 50)
GREEN = (50, 220, 50)
BLUE = (50, 100, 255)
BLACK = (0, 0, 0)
DARK_GRAY = (40, 40, 40)
LIGHT_GRAY = (180, 180, 180)
GOLD = (255, 215, 0)
ORANGE = (255, 165, 0)
PURPLE = (138, 43, 226)

# Fonts
FONT_LARGE = pygame.font.Font(None, 36)
FONT_MED = pygame.font.Font(None, 28)
FONT_SMALL = pygame.font.Font(None, 20)

# Attack types
ATTACK_TYPES = {
    "quick": (5, 10, BLUE),
    "normal": (10, 20, WHITE),
    "heavy": (15, 25, ORANGE)
}

# Animation system
class Animation:
    def __init__(self):
        self.animations = []
    
    def add_damage_popup(self, x, y, damage, is_crit=False):
        color = RED if is_crit else WHITE
        self.animations.append({
            'type': 'damage',
            'x': x, 'y': y,
            'text': str(damage),
            'color': color,
            'timer': 60,
            'offset_y': 0
        })
    
    def add_heal_popup(self, x, y, heal_amount):
        self.animations.append({
            'type': 'heal',
            'x': x, 'y': y,
            'text': f"+{heal_amount}",
            'color': GREEN,
            'timer': 60,
            'offset_y': 0
        })
    
    def add_shake(self, player):
        self.animations.append({
            'type': 'shake',
            'player': player,
            'timer': 20,
            'intensity': 5
        })
    
    def add_glow(self, player, color):
        self.animations.append({
            'type': 'glow',
            'player': player,
            'color': color,
            'timer': 30,
            'pulse': 0
        })
    
    def update(self):
        for anim in self.animations[:]:
            anim['timer'] -= 1
            if anim['type'] in ['damage', 'heal']:
                anim['offset_y'] -= 2
            elif anim['type'] == 'glow':
                anim['pulse'] += 0.2
            
            if anim['timer'] <= 0:
                self.animations.remove(anim)
    
    def draw(self, screen):
        for anim in self.animations:
            if anim['type'] in ['damage', 'heal']:
                alpha = min(255, anim['timer'] * 4)
                text_surf = FONT_MED.render(anim['text'], True, anim['color'])
                text_surf.set_alpha(alpha)
                screen.blit(text_surf, (anim['x'], anim['y'] + anim['offset_y']))

animation_manager = Animation()

# Game state
def reset_game():
    return {
        "p1_hp": 100,
        "p2_hp": 100,
        "p1_def": False,
        "p2_def": False,
        "p1_special": False,
        "p2_special": False,
        "turn": 1,
        "log": [],
        "game_over": False,
        "winner": None,
        "p1_shake": 0,
        "p2_shake": 0
    }

game_state = reset_game()

# Enhanced Button class
class Button:
    def __init__(self, text, x, y, w, h, action, color=DARK_GRAY, text_color=WHITE, disabled_check=None):
        self.rect = pygame.Rect(x, y, w, h)
        self.text = text
        self.action = action
        self.color = color
        self.text_color = text_color
        self.hover = False
        self.disabled_check = disabled_check
        self.pulse = 0

    def update(self, mouse_pos):
        self.hover = self.rect.collidepoint(mouse_pos)
        self.pulse += 0.1
    
    def is_disabled(self):
        return self.disabled_check() if self.disabled_check else False

    def draw(self, screen):
        disabled = self.is_disabled()
        
        # Button color with hover effect
        if disabled:
            color = (60, 60, 60)
            text_color = (120, 120, 120)
        else:
            if self.hover:
                color = tuple(min(255, c + 30) for c in self.color)
            else:
                color = self.color
            text_color = self.text_color
        
        # Draw button with rounded corners effect
        pygame.draw.rect(screen, color, self.rect)
        pygame.draw.rect(screen, WHITE if not disabled else (80, 80, 80), self.rect, 2)
        
        # Add subtle glow effect when hovering
        if self.hover and not disabled:
            glow_rect = self.rect.inflate(4, 4)
            pygame.draw.rect(screen, color, glow_rect, 2)
        
        # Draw text
        txt_surf = FONT_SMALL.render(self.text, True, text_color)
        text_x = self.rect.x + (self.rect.width - txt_surf.get_width()) // 2
        text_y = self.rect.y + (self.rect.height - txt_surf.get_height()) // 2
        screen.blit(txt_surf, (text_x, text_y))

    def is_clicked(self, pos):
        return self.rect.collidepoint(pos) and not self.is_disabled()

# Enhanced HP Bar
def draw_hp_bar(screen, x, y, current_hp, max_hp, player_num):
    bar_width = 200
    bar_height = 25
    
    # Background
    bg_rect = pygame.Rect(x, y, bar_width, bar_height)
    pygame.draw.rect(screen, DARK_GRAY, bg_rect)
    pygame.draw.rect(screen, WHITE, bg_rect, 2)
    
    # HP fill
    hp_percentage = max(0, current_hp / max_hp)
    fill_width = int(bar_width * hp_percentage)
    
    # Color based on HP percentage
    if hp_percentage > 0.6:
        hp_color = GREEN
    elif hp_percentage > 0.3:
        hp_color = ORANGE
    else:
        hp_color = RED
    
    if fill_width > 0:
        fill_rect = pygame.Rect(x, y, fill_width, bar_height)
        pygame.draw.rect(screen, hp_color, fill_rect)
    
    # HP text
    hp_text = f"{max(0, current_hp)}/{max_hp}"
    text_surf = FONT_SMALL.render(hp_text, True, WHITE)
    text_x = x + (bar_width - text_surf.get_width()) // 2
    text_y = y + (bar_height - text_surf.get_height()) // 2
    screen.blit(text_surf, (text_x, text_y))
    
    # Player label
    label = f"Player {player_num}"
    label_surf = FONT_MED.render(label, True, WHITE)
    screen.blit(label_surf, (x, y - 30))

# Action functions with animations
def log_action(msg):
    game_state["log"].append(msg)
    if len(game_state["log"]) > 5:
        game_state["log"].pop(0)

def next_turn():
    game_state["turn"] = 2 if game_state["turn"] == 1 else 1

def attack(attack_type="normal"):
    if game_state["game_over"]:
        return

    if attack_type not in ATTACK_TYPES:
        attack_type = "normal"

    min_dmg, max_dmg, color = ATTACK_TYPES[attack_type]
    damage = random.randint(min_dmg, max_dmg)
    crit = False

    if random.random() < 0.2:
        damage *= 2
        crit = True

    if game_state["turn"] == 1:
        if game_state["p2_def"]:
            damage //= 2
            game_state["p2_def"] = False
            log_action("Player 2 defended! Damage halved.")
        game_state["p2_hp"] -= damage
        log_action(f"Player 1 {'CRIT ' if crit else ''}used {attack_type} attack for {damage}!")
        
        # Add animations
        animation_manager.add_damage_popup(720, 200, damage, crit)
        animation_manager.add_shake(2)
        animation_manager.add_glow(1, color)
    else:
        if game_state["p1_def"]:
            damage //= 2
            game_state["p1_def"] = False
            log_action("Player 1 defended! Damage halved.")
        game_state["p1_hp"] -= damage
        log_action(f"Player 2 {'CRIT ' if crit else ''}used {attack_type} attack for {damage}!")
        
        # Add animations
        animation_manager.add_damage_popup(170, 200, damage, crit)
        animation_manager.add_shake(1)
        animation_manager.add_glow(2, color)

    # Check for game over
    if game_state["p1_hp"] <= 0:
        game_state["game_over"] = True
        game_state["winner"] = 2
    elif game_state["p2_hp"] <= 0:
        game_state["game_over"] = True
        game_state["winner"] = 1

    next_turn()

def defend():
    if game_state["turn"] == 1:
        game_state["p1_def"] = True
        log_action("Player 1 is defending. 🛡️")
        animation_manager.add_glow(1, BLUE)
    else:
        game_state["p2_def"] = True
        log_action("Player 2 is defending. 🛡️")
        animation_manager.add_glow(2, BLUE)
    next_turn()

def heal():
    heal_amt = random.randint(10, 20)
    if game_state["turn"] == 1:
        if game_state["p1_hp"] >= 100:
            log_action("Player 1 is already at full HP.")
            return
        game_state["p1_hp"] = min(game_state["p1_hp"] + heal_amt, 100)
        log_action(f"Player 1 healed for {heal_amt}. ❤️")
        animation_manager.add_heal_popup(170, 200, heal_amt)
        animation_manager.add_glow(1, GREEN)
    else:
        if game_state["p2_hp"] >= 100:
            log_action("Player 2 is already at full HP.")
            return
        game_state["p2_hp"] = min(game_state["p2_hp"] + heal_amt, 100)
        log_action(f"Player 2 healed for {heal_amt}. ❤️")
        animation_manager.add_heal_popup(720, 200, heal_amt)
        animation_manager.add_glow(2, GREEN)
    next_turn()

def special():
    if game_state["turn"] == 1:
        if game_state["p1_special"]:
            log_action("Player 1 already used special.")
            return
        damage = random.randint(25, 35)
        if game_state["p2_def"]:
            damage //= 2
            game_state["p2_def"] = False
        game_state["p2_hp"] -= damage
        game_state["p1_special"] = True
        log_action(f"Player 1 used SPECIAL for {damage}! 🔥")
        
        animation_manager.add_damage_popup(720, 200, damage, True)
        animation_manager.add_shake(2)
        animation_manager.add_glow(1, PURPLE)
    else:
        if game_state["p2_special"]:
            log_action("Player 2 already used special.")
            return
        damage = random.randint(25, 35)
        if game_state["p1_def"]:
            damage //= 2
            game_state["p1_def"] = False
        game_state["p1_hp"] -= damage
        game_state["p2_special"] = True
        log_action(f"Player 2 used SPECIAL for {damage}! 🔥")
        
        animation_manager.add_damage_popup(170, 200, damage, True)
        animation_manager.add_shake(1)
        animation_manager.add_glow(2, PURPLE)

    # Check for game over
    if game_state["p1_hp"] <= 0:
        game_state["game_over"] = True
        game_state["winner"] = 2
    elif game_state["p2_hp"] <= 0:
        game_state["game_over"] = True
        game_state["winner"] = 1

    next_turn()

def restart_game():
    global game_state
    game_state = reset_game()
    animation_manager.animations.clear()

# Enhanced Buttons
game_buttons = [
    Button("Quick", 50, 450, 80, 40, lambda: attack("quick"), BLUE),
    Button("Normal", 140, 450, 80, 40, lambda: attack("normal"), DARK_GRAY),
    Button("Heavy", 230, 450, 80, 40, lambda: attack("heavy"), ORANGE),
    Button("Defend", 320, 450, 80, 40, defend, GREEN),
    Button("Heal", 410, 450, 80, 40, heal, (255, 100, 150), disabled_check=lambda: (game_state["turn"] == 1 and game_state["p1_hp"] >= 100) or (game_state["turn"] == 2 and game_state["p2_hp"] >= 100)),
    Button("Special", 500, 450, 90, 40, special, PURPLE, disabled_check=lambda: (game_state["turn"] == 1 and game_state["p1_special"]) or (game_state["turn"] == 2 and game_state["p2_special"])),
]

restart_button = Button("🔄 Restart Game", 325, 480, 200, 60, restart_game, GREEN, BLACK)

def draw_player_with_effects(screen, img, x, y, player_num):
    # Apply shake effect
    shake_offset = 0
    for anim in animation_manager.animations:
        if anim['type'] == 'shake' and anim['player'] == player_num:
            shake_offset = random.randint(-anim['intensity'], anim['intensity'])
            break
    
    # Draw glow effect
    for anim in animation_manager.animations:
        if anim['type'] == 'glow' and anim['player'] == player_num:
            glow_size = int(20 + 10 * math.sin(anim['pulse']))
            glow_surf = pygame.Surface((img.get_width() + glow_size, img.get_height() + glow_size))
            glow_surf.set_alpha(50)
            glow_surf.fill(anim['color'])
            screen.blit(glow_surf, (x - glow_size//2 + shake_offset, y - glow_size//2))
            break
    
    # Draw player
    screen.blit(img, (x + shake_offset, y))
    
    # Draw status indicators
    status_y = y + img.get_height() + 10
    if (player_num == 1 and game_state["p1_def"]) or (player_num == 2 and game_state["p2_def"]):
        shield_text = FONT_SMALL.render("🛡️ DEFENDING", True, BLUE)
        screen.blit(shield_text, (x, status_y))

def draw_game():
    # Gradient background
    for y in range(HEIGHT):
        color_intensity = int(30 + 20 * (y / HEIGHT))
        color = (color_intensity // 3, color_intensity // 4, color_intensity)
        pygame.draw.line(screen, color, (0, y), (WIDTH, y))
    
    # Draw players with effects
    draw_player_with_effects(screen, PLAYER1_IMG, 100, 150, 1)
    draw_player_with_effects(screen, PLAYER2_IMG, 650, 150, 2)

    # Enhanced HP bars
    draw_hp_bar(screen, 100, 100, game_state["p1_hp"], 100, 1)
    draw_hp_bar(screen, 500, 100, game_state["p2_hp"], 100, 2)

    # Turn indicator
    if not game_state["game_over"]:
        turn_text = f"Player {game_state['turn']}'s Turn"
        turn_surf = FONT_LARGE.render(turn_text, True, GOLD)
        turn_rect = turn_surf.get_rect(center=(WIDTH//2, 50))
        # Add background for better visibility
        bg_rect = turn_rect.inflate(20, 10)
        pygame.draw.rect(screen, DARK_GRAY, bg_rect)
        pygame.draw.rect(screen, GOLD, bg_rect, 2)
        screen.blit(turn_surf, turn_rect)

    # Enhanced action log
    log_bg = pygame.Rect(50, 320, 400, 120)
    pygame.draw.rect(screen, (20, 20, 20, 180), (0, 0, 0, 0))
    pygame.draw.rect(screen, WHITE, log_bg, 2)
    
    log_title = FONT_MED.render("⚔️ Battle Log", True, WHITE)
    screen.blit(log_title, (60, 325))
    
    y = 350
    for i, line in enumerate(game_state["log"]):
        alpha = 255 - (len(game_state["log"]) - i - 1) * 40
        text_surf = FONT_SMALL.render(line, True, WHITE)
        text_surf.set_alpha(max(100, alpha))
        screen.blit(text_surf, (60, y))
        y += 22

    # Update and draw button hover effects
    mouse_pos = pygame.mouse.get_pos()
    
    # Buttons
    if not game_state["game_over"]:
        for btn in game_buttons:
            btn.update(mouse_pos)
            btn.draw(screen)
    else:
        # Game over screen
        winner_text = f"🎉 Player {game_state['winner']} Wins! 🎉"
        winner_surf = FONT_LARGE.render(winner_text, True, GOLD)
        winner_rect = winner_surf.get_rect(center=(WIDTH//2, 350))
        
        # Winner background
        bg_rect = winner_rect.inflate(40, 20)
        pygame.draw.rect(screen, DARK_GRAY, bg_rect)
        pygame.draw.rect(screen, GOLD, bg_rect, 3)
        screen.blit(winner_surf, winner_rect)
        
        restart_button.update(mouse_pos)
        restart_button.draw(screen)

    # Draw animations last
    animation_manager.draw(screen)

def run_game():
    clock = pygame.time.Clock()
    running = True
    
    while running:
        # Update animations
        animation_manager.update()
        
        # Clear screen and draw game
        screen.fill(BLACK)
        draw_game()
        
        # Handle events
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN:
                if not game_state["game_over"]:
                    for btn in game_buttons:
                        if btn.is_clicked(event.pos):
                            btn.action()
                            break
                else:
                    if restart_button.is_clicked(event.pos):
                        restart_button.action()
        
        pygame.display.flip()
        clock.tick(60)
    
    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    run_game()