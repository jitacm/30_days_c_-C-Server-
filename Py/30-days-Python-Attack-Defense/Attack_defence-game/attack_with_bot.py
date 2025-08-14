import pygame
import random
import time
import os

# Initialize Pygame
pygame.init()

# Constants
SCREEN_WIDTH = 1000
SCREEN_HEIGHT = 700
FPS = 60

# Colors
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
GRAY = (128, 128, 128)
LIGHT_GRAY = (200, 200, 200)
DARK_GRAY = (64, 64, 64)

class BattleGame:
    def __init__(self):
        self.screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption("Battle Game")
        self.clock = pygame.time.Clock()
        self.font = pygame.font.Font(None, 36)
        self.big_font = pygame.font.Font(None, 48)
        
        # Load images
        self.load_images()
        
        # Game state
        self.reset_game()
        self.game_over = False
        self.winner = None
        self.animation_timer = 0
        self.show_damage = False
        self.damage_text = ""
        self.damage_timer = 0
        
        # Buttons
        self.attack_button = pygame.Rect(50, 600, 120, 50)
        self.defend_button = pygame.Rect(200, 600, 120, 50)
        self.restart_button = pygame.Rect(SCREEN_WIDTH//2 - 75, 500, 150, 50)
        
    def load_images(self):
        try:
            BASE_PATH = os.path.join(os.path.dirname(__file__), "assets")
            self.player1_img = pygame.image.load(os.path.join(BASE_PATH, "P1_Shot.png"))
            self.player2_img = pygame.image.load(os.path.join(BASE_PATH, "P2_Shot.png"))
            
            # Scale images to appropriate size
            self.player1_img = pygame.transform.scale(self.player1_img, (150, 150))
            self.player2_img = pygame.transform.scale(self.player2_img, (150, 150))
            
        except pygame.error:
            # Create placeholder rectangles if images not found
            self.player1_img = pygame.Surface((150, 150))
            self.player1_img.fill(BLUE)
            self.player2_img = pygame.Surface((150, 150))
            self.player2_img.fill(RED)
            
        # Store original images for animation
        self.player1_original = self.player1_img.copy()
        self.player2_original = self.player2_img.copy()
        
    def reset_game(self):
        self.player1_hp = 100
        self.player2_hp = 100
        self.player1_defending = False
        self.player2_defending = False
        self.turn = 1
        self.game_over = False
        self.winner = None
        self.animation_timer = 0
        self.show_damage = False
        self.damage_timer = 0
        
    def computer_choice(self):
        if self.player1_defending:
            return 'attack'
        elif self.player2_defending:
            return 'attack'
        elif self.player1_hp < 30:
            return 'attack'
        elif self.player2_hp < 20:
            return 'defend'
        elif random.random() < 0.7:
            return 'attack'
        return 'defend'
    
    def process_attack(self, attacker, defender_hp, defender_defending):
        damage = random.randint(10, 20)
        if defender_defending:
            damage //= 2
        new_hp = max(0, defender_hp - damage)
        
        # Start animation
        self.animation_timer = pygame.time.get_ticks()
        self.show_damage = True
        self.damage_text = f"-{damage}"
        self.damage_timer = pygame.time.get_ticks()
        
        return new_hp, damage
    
    def update_animation(self):
        current_time = pygame.time.get_ticks()
        
        # Reset defending status after animation
        if self.animation_timer > 0 and current_time - self.animation_timer > 1000:
            self.player1_defending = False
            self.player2_defending = False
            self.animation_timer = 0
            
        # Hide damage text after delay
        if self.show_damage and current_time - self.damage_timer > 1500:
            self.show_damage = False
    
    def draw_health_bar(self, x, y, width, height, hp, max_hp, label):
        # Background
        pygame.draw.rect(self.screen, GRAY, (x, y, width, height))
        
        # Health bar
        health_width = int((hp / max_hp) * width)
        color = GREEN if hp > 50 else (255, 165, 0) if hp > 25 else RED
        pygame.draw.rect(self.screen, color, (x, y, health_width, height))
        
        # Border
        pygame.draw.rect(self.screen, BLACK, (x, y, width, height), 2)
        
        # Text
        text = self.font.render(f"{label}: {hp}/100", True, BLACK)
        self.screen.blit(text, (x, y - 30))
    
    def draw_button(self, rect, text, color=LIGHT_GRAY, text_color=BLACK, enabled=True):
        if not enabled:
            color = GRAY
            text_color = DARK_GRAY
            
        pygame.draw.rect(self.screen, color, rect)
        pygame.draw.rect(self.screen, BLACK, rect, 2)
        
        text_surface = self.font.render(text, True, text_color)
        text_rect = text_surface.get_rect(center=rect.center)
        self.screen.blit(text_surface, text_rect)
    
    def draw_player(self, image, x, y, defending=False, attacking=False):
        # Create animation effects
        display_image = image.copy()
        
        if defending:
            # Shield effect - blue tint
            shield_surface = pygame.Surface(display_image.get_size())
            shield_surface.fill((0, 0, 255))
            shield_surface.set_alpha(50)
            display_image.blit(shield_surface, (0, 0))
            
        if attacking and self.animation_timer > 0:
            # Attack animation - shake effect
            shake_x = random.randint(-5, 5)
            shake_y = random.randint(-5, 5)
            x += shake_x
            y += shake_y
            
            # Red tint for attack
            attack_surface = pygame.Surface(display_image.get_size())
            attack_surface.fill((255, 0, 0))
            attack_surface.set_alpha(30)
            display_image.blit(attack_surface, (0, 0))
        
        self.screen.blit(display_image, (x, y))
    
    def draw(self):
        self.screen.fill(WHITE)
        
        # Title
        title = self.big_font.render("BATTLE GAME", True, BLACK)
        title_rect = title.get_rect(center=(SCREEN_WIDTH//2, 50))
        self.screen.blit(title, title_rect)
        
        # Health bars
        self.draw_health_bar(100, 120, 300, 30, self.player1_hp, 100, "Player 1")
        self.draw_health_bar(600, 120, 300, 30, self.player2_hp, 100, "Player 2 (Bot)")
        
        # Players
        player1_attacking = (self.turn == 1 and self.animation_timer > 0)
        player2_attacking = (self.turn == 2 and self.animation_timer > 0)
        
        self.draw_player(self.player1_img, 150, 200, self.player1_defending, player1_attacking)
        self.draw_player(self.player2_img, 650, 200, self.player2_defending, player2_attacking)
        
        # Turn indicator
        if not self.game_over:
            turn_text = f"Turn: {'Player 1' if self.turn == 1 else 'Player 2 (Bot)'}"
            turn_surface = self.font.render(turn_text, True, BLACK)
            self.screen.blit(turn_surface, (SCREEN_WIDTH//2 - 100, 400))
        
        # Action buttons (only show for player 1's turn)
        if self.turn == 1 and not self.game_over:
            self.draw_button(self.attack_button, "ATTACK", GREEN)
            self.draw_button(self.defend_button, "DEFEND", BLUE)
        
        # Status indicators
        if self.player1_defending:
            status = self.font.render("🛡️ DEFENDING", True, BLUE)
            self.screen.blit(status, (150, 370))
            
        if self.player2_defending:
            status = self.font.render("🛡️ DEFENDING", True, BLUE)
            self.screen.blit(status, (650, 370))
        
        # Damage text
        if self.show_damage:
            damage_surface = self.big_font.render(self.damage_text, True, RED)
            pos_x = 650 if self.turn == 1 else 150
            self.screen.blit(damage_surface, (pos_x + 75, 180))
        
        # Game over screen
        if self.game_over:
            # Semi-transparent overlay
            overlay = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT))
            overlay.set_alpha(128)
            overlay.fill(BLACK)
            self.screen.blit(overlay, (0, 0))
            
            # Winner text
            winner_text = self.big_font.render(f"{self.winner} WINS! 🏆", True, WHITE)
            winner_rect = winner_text.get_rect(center=(SCREEN_WIDTH//2, 300))
            self.screen.blit(winner_text, winner_rect)
            
            # Restart button
            self.draw_button(self.restart_button, "RESTART", GREEN, BLACK)
        
        pygame.display.flip()
    
    def handle_player_action(self, action):
        if action == 'attack':
            self.player2_hp, damage = self.process_attack(1, self.player2_hp, self.player2_defending)
            print(f"Player 1 attacks for {damage} damage!")
        elif action == 'defend':
            self.player1_defending = True
            print("Player 1 is defending!")
        
        # Check if game is over
        if self.player2_hp <= 0:
            self.game_over = True
            self.winner = "Player 1"
        else:
            self.turn = 2
    
    def handle_bot_turn(self):
        action = self.computer_choice()
        print(f"Bot chooses: {action}")
        
        if action == 'attack':
            self.player1_hp, damage = self.process_attack(2, self.player1_hp, self.player1_defending)
            print(f"Bot attacks for {damage} damage!")
        elif action == 'defend':
            self.player2_defending = True
            print("Bot is defending!")
        
        # Check if game is over
        if self.player1_hp <= 0:
            self.game_over = True
            self.winner = "Player 2 (Bot)"
        else:
            self.turn = 1
    
    def run(self):
        running = True
        bot_action_timer = 0
        
        while running:
            current_time = pygame.time.get_ticks()
            
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                
                elif event.type == pygame.MOUSEBUTTONDOWN:
                    mouse_pos = pygame.mouse.get_pos()
                    
                    if self.game_over and self.restart_button.collidepoint(mouse_pos):
                        self.reset_game()
                    
                    elif self.turn == 1 and not self.game_over:
                        if self.attack_button.collidepoint(mouse_pos):
                            self.handle_player_action('attack')
                            bot_action_timer = current_time + 2000  # Bot acts after 2 seconds
                        
                        elif self.defend_button.collidepoint(mouse_pos):
                            self.handle_player_action('defend')
                            bot_action_timer = current_time + 2000  # Bot acts after 2 seconds
            
            # Handle bot turn with delay
            if (self.turn == 2 and not self.game_over and 
                current_time >= bot_action_timer and bot_action_timer > 0):
                self.handle_bot_turn()
                bot_action_timer = 0
            
            # Update animations
            self.update_animation()
            
            # Draw everything
            self.draw()
            self.clock.tick(FPS)
        
        pygame.quit()

if __name__ == "__main__":
    game = BattleGame()
    game.run()