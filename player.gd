extends CharacterBody3D


const SPEED = 5.0
const SPRINT_SPEED = 8.0
const CROUCH_SPEED = 2.5
const JUMP_VELOCITY = 4.5
# Nastavení rychlostí při pohybu


@onready var neck: Node3D = $neck
@onready var collision_shape: CollisionShape3D = $CollisionShape3D


const MOUSE_SENS: float = 0.01
const STAND_HEIGHT: float = 1.0
const CROUCH_HEIGHT: float = 0.5

# Nastavení hitboxu
const STAND_COLLISION_HEIGHT: float = 2.0
const CROUCH_COLLISION_HEIGHT: float = 1.0
#Zde jdou změnit velikosti hitboxů


var is_crouching: bool = false
var is_sprinting: bool = false


func _input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		var mouse_motion: Vector2 = event.relative
		rotate_y(-(mouse_motion.x * MOUSE_SENS))
		neck.rotate_x(-(mouse_motion.y * MOUSE_SENS))
		neck.rotation.x = deg_to_rad(clamp(rad_to_deg(neck.rotation.x), -60, 40))
		# Kód pro otáčení hráče myší.


func _physics_process(delta: float) -> void:
	# Gravity
	if not is_on_floor():
		velocity += get_gravity() * delta


	# Jump
	if Input.is_action_just_pressed("jump") and is_on_floor() and not is_crouching:
		velocity.y = JUMP_VELOCITY


	# Kód pro skrčení
	if Input.is_action_pressed("crouch"):
		is_crouching = true
	else:
		is_crouching = false


	# Pohyb kamery při skrčení
	if is_crouching:
		neck.position.y = CROUCH_HEIGHT
	else:
		neck.position.y = STAND_HEIGHT

	#Měnič hitboxů
	var capsule = collision_shape.shape as CapsuleShape3D

	if capsule:
		if is_crouching:
			capsule.height = CROUCH_COLLISION_HEIGHT
		else:
			capsule.height = STAND_COLLISION_HEIGHT


	# Movement
	var input_dir := Input.get_vector("left", "right", "forward", "backward")


	# Sprint
	if Input.is_action_pressed("sprint") and input_dir.y < 0 and not is_crouching:
		is_sprinting = true
	else:
		is_sprinting = false


	# Výběr rychlosti a měnič pohybových módů
	var current_speed = SPEED

	if is_crouching:
		current_speed = CROUCH_SPEED
	elif is_sprinting:
		current_speed = SPRINT_SPEED


	var direction := (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()


	if direction:
		velocity.x = direction.x * current_speed
		velocity.z = direction.z * current_speed
	else:
		velocity.x = move_toward(velocity.x, 0, current_speed)
		velocity.z = move_toward(velocity.z, 0, current_speed)


	move_and_slide()
