// Copyright Epic Games, Inc. All Rights Reserved.

#include "DungeonCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Dungeon.h" 
#include "CollectableItem.h"
#include "Lock.h"
ADungeonCharacter::ADungeonCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void ADungeonCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ADungeonCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADungeonCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADungeonCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADungeonCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ADungeonCharacter::LookInput);

		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ADungeonCharacter::Interact);
	}
	else
	{
		UE_LOG(LogDungeon, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ADungeonCharacter::Interact()
{
	UE_LOG(LogTemp, Display, TEXT("Interacting"));

	FVector Start = FirstPersonCameraComponent->GetComponentLocation();
	FVector End = Start + (FirstPersonCameraComponent->GetForwardVector() * MaxInteractionDistance);
	DrawDebugLine(GetWorld(), Start,End,FColor::Red,false,5.0f);

	FCollisionShape InteractionSphere = FCollisionShape::MakeSphere(InteractionSphereRadius);
	DrawDebugSphere(GetWorld(), End,InteractionSphereRadius,20,FColor::Blue, false, 5.0f);

	FHitResult HitResult;
	bool hasHit = GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_GameTraceChannel2, InteractionSphere);

	if (hasHit) {
		AActor* HitActor = HitResult.GetActor();

		if (HitActor->ActorHasTag("CollectableItem")) {
			ACollectableItem* CollectableItem = Cast<ACollectableItem>(HitActor);
			if (CollectableItem) {
				ItemList.Add(CollectableItem->ItemName);
				CollectableItem->Destroy();
				if (PickSound) {
					int randNum1 = rand() % (140 - 60 + 1) + 60;
					float pitch = randNum1 * 0.01;
					int randNum2 = rand() % (130 - 100 + 1) + 100;
					float volume = randNum2 * 0.01;
					UGameplayStatics::PlaySoundAtLocation(GetWorld(), PickSound, GetActorLocation(), volume, pitch);
				}
			}
		}
		else if (HitActor->ActorHasTag("Lock")) {
			UE_LOG(LogTemp, Warning, TEXT("Successfully hit the Lock Actor!"));
			ALock* LockItem = Cast<ALock>(HitActor);
			if (LockItem) {
				if (!LockItem->getKeyPlaced()) {
					int32 ItemRemoved = ItemList.RemoveSingle(LockItem->KeyItemName);
					UE_LOG(LogTemp, Warning, TEXT("Items removed from inventory: %d"), ItemRemoved);
					if (ItemRemoved) {
						LockItem->setKeyPlaced(true);
						UE_LOG(LogTemp, Warning, TEXT("Key successfully placed on lock!"));
					}
					else {
						UE_LOG(LogTemp, Error, TEXT("Failed to place key! Player does not have an item matching the Lock's required name."));
					}
				}
				else {
					ItemList.Add(LockItem->KeyItemName);
					LockItem->setKeyPlaced(false);
				}
			}
		}
	}
}


void ADungeonCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ADungeonCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void ADungeonCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ADungeonCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ADungeonCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void ADungeonCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
