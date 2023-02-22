#pragma once

#include "GameFramework/Character.h"
#include "Settings/AlsMantlingSettings.h"
#include "State/AlsLocomotionState.h"
#include "State/AlsRagdollingState.h"
#include "State/AlsRollingState.h"
#include "State/AlsViewState.h"
#include "Utility/AlsGameplayTags.h"
#include "AlsComponent.generated.h"

class UAlsCharacterMovementComponent;
class UAlsCharacterSettings;
class UAlsMovementSettings;
class UAlsAnimationInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOverlayModeChangedSignature,const FGameplayTag&, PreviousOverlayMode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMantlingStartedSignature,const FAlsMantlingParameters&, Parameters);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSimpleSignature);



UCLASS(ClassGroup="Als", BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent),
	AutoExpandCategories = ("Settings|Als Character", "Settings|Als Character|Desired State", "State|Als Character"))
class ALS_API UAlsComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UAlsCharacterMovementComponent> AlsCharacterMovement;

	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character")
	TObjectPtr<UAlsCharacterSettings> Settings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character")
	TObjectPtr<UAlsMovementSettings> MovementSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State",
		ReplicatedUsing = "OnReplicated_DesiredAiming")
	bool bDesiredAiming;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State", Replicated, meta=(Categories="Als.RotationMode"))
	FGameplayTag DesiredRotationMode{AlsRotationModeTags::LookingDirection};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State", Replicated, meta=(Categories="Als.Stance"))
	FGameplayTag DesiredStance{AlsStanceTags::Standing};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State", Replicated, meta=(Categories="Als.Gait"))
	FGameplayTag DesiredGait{AlsGaitTags::Running};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State", Replicated, meta=(Categories="Als.ViewMode"))
	FGameplayTag ViewMode{AlsViewModeTags::ThirdPerson};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State",
		ReplicatedUsing = "OnReplicated_OverlayMode", meta=(Categories="Als.OverlayMode"))
	FGameplayTag OverlayMode{AlsOverlayModeTags::Default};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	bool bSimulatedProxyTeleported;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient, Meta = (ShowInnerProperties))
	TWeakObjectPtr<UAlsAnimationInstance> AnimationInstance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag LocomotionMode{AlsLocomotionModeTags::Grounded};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag RotationMode{AlsRotationModeTags::LookingDirection};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag Stance{AlsStanceTags::Standing};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag Gait{AlsGaitTags::Walking};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag LocomotionAction;

	// Raw replicated view rotation. For smooth rotation use FAlsViewState::Rotation.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient,
		ReplicatedUsing = "OnReplicated_RawViewRotation")
	FRotator RawViewRotation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsViewState ViewState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient, Replicated)
	FVector_NetQuantizeNormal InputDirection;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsLocomotionState LocomotionState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	int32 MantlingRootMotionSourceId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient, Replicated)
	FVector_NetQuantize100 RagdollTargetLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsRagdollingState RagdollingState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsRollingState RollingState;

	FTimerHandle BrakingFrictionFactorResetTimer;

public:
	explicit UAlsComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// #if WITH_EDITOR
	// 	virtual bool CanEditChange(const FProperty* Property) const override;
	// #endif

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnRegister() override;

	UFUNCTION(BlueprintPure, Category = "Als Character")
	static UAlsComponent* FindAlsComponent(const AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Als Character", meta=(DisplayName="Find AlsComponent", ExpandBoolAsExecs = "ReturnValue"))
	static bool K2_FindAlsComponent(const AActor* Actor, UAlsComponent*& Instance);

	UPROPERTY(BlueprintAssignable, Category="Als Event")
	FOverlayModeChangedSignature OnOverlayModeChangedEvent;

	UPROPERTY(BlueprintAssignable, Category="Als Event")
	FMantlingStartedSignature OnMantlingStartedEvent;

	UPROPERTY(BlueprintAssignable, Category="Als Event")
	FSimpleSignature OnMantlingEndedEvent;

	UPROPERTY(BlueprintAssignable, Category="Als Event")
	FSimpleSignature OnRagdollingStartedEvent;
	
	UPROPERTY(BlueprintAssignable, Category="Als Event")
	FSimpleSignature OnRagdollingEndedEvent;
	

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	//call from owner.
	virtual void PostNetReceiveLocationAndRotation();

	//call from owner.
	virtual void OnRep_ReplicatedBasedMovement();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//call from owner.
	virtual void PossessedBy(AController* NewController);

	//call from owner
	virtual void Restart();

private:
	void RefreshVisibilityBasedAnimTickOption() const;

public:
	bool IsSimulatedProxyTeleported() const;

	// View Mode

public:
	const FGameplayTag& GetViewMode() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character", Meta = (AutoCreateRefTerm = "NewViewMode"))
	void SetViewMode(UPARAM(meta=(Categories="Als.ViewMode")) const FGameplayTag& NewViewMode);

private:
	UFUNCTION(Server, Reliable)
	void ServerSetViewMode(const FGameplayTag& NewViewMode);
	virtual void ServerSetViewMode_Implementation(const FGameplayTag& NewViewMode);


	// Locomotion Mode

public:
	UFUNCTION()
	virtual void OnMovementModeChanged(ACharacter* InCharacter, EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0);

public:
	const FGameplayTag& GetLocomotionMode() const;

private:
	void SetLocomotionMode(const FGameplayTag& NewLocomotionMode);

	void NotifyLocomotionModeChanged(const FGameplayTag& PreviousLocomotionMode);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnLocomotionModeChanged(const FGameplayTag& PreviousLocomotionMode);
	virtual void OnLocomotionModeChanged_Implementation(const FGameplayTag& PreviousLocomotionMode);

	// Desired Aiming

public:
	bool IsDesiredAiming() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character")
	void SetDesiredAiming(bool bNewDesiredAiming);

private:
	UFUNCTION(Server, Reliable)
	void ServerSetDesiredAiming(bool bNewDesiredAiming);
	virtual void ServerSetDesiredAiming_Implementation(bool bNewDesiredAiming);


	UFUNCTION()
	void OnReplicated_DesiredAiming(bool bPreviousDesiredAiming);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnDesiredAimingChanged(bool bPreviousDesiredAiming);
	virtual void OnDesiredAimingChanged_Implementation(bool bPreviousDesiredAiming);


	// Desired Rotation Mode

public:
	const FGameplayTag& GetDesiredRotationMode() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character", Meta = (AutoCreateRefTerm = "NewDesiredRotationMode"))
	void SetDesiredRotationMode(UPARAM(meta=(Categories="Als.RotationMode")) const FGameplayTag& NewDesiredRotationMode);

private:
	UFUNCTION(Server, Reliable)
	void ServerSetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode);
	virtual void ServerSetDesiredRotationMode_Implementation(const FGameplayTag& NewDesiredRotationMode);

	// Rotation Mode

public:
	const FGameplayTag& GetRotationMode() const;

private:
	void SetRotationMode(const FGameplayTag& NewRotationMode);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnRotationModeChanged(const FGameplayTag& PreviousRotationMode);
	virtual void OnRotationModeChanged_Implementation(const FGameplayTag& PreviousRotationMode);

	void RefreshRotationMode();

	// Desired Stance

public:
	const FGameplayTag& GetDesiredStance() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character", Meta = (AutoCreateRefTerm = "NewDesiredStance"))
	void SetDesiredStance(UPARAM(meta=(Categories="Als.Stance")) const FGameplayTag& NewDesiredStance);

private:
	UFUNCTION(Server, Reliable)
	void ServerSetDesiredStance(const FGameplayTag& NewDesiredStance);
	virtual void ServerSetDesiredStance_Implementation(const FGameplayTag& NewDesiredStance);


protected:
	virtual void ApplyDesiredStance();

	// Stance

public:
	// call from ownerCharacter
	// virtual bool CanCrouch() const;

	// call from ownerCharacter.
	UFUNCTION(BlueprintCallable, Category = "Als Character")
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	// call from ownerCharacter.
	UFUNCTION(BlueprintCallable, Category = "Als Character")
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

public:
	const FGameplayTag& GetStance() const;

private:
	void SetStance(const FGameplayTag& NewStance);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnStanceChanged(const FGameplayTag& PreviousStance);
	virtual void OnStanceChanged_Implementation(const FGameplayTag& PreviousStance);
	// Desired Gait

public:
	const FGameplayTag& GetDesiredGait() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character", Meta = (AutoCreateRefTerm = "NewDesiredGait"))
	void SetDesiredGait(UPARAM(meta=(Categories="Als.Gait")) const FGameplayTag& NewDesiredGait);

private:
	UFUNCTION(Server, Reliable)
	void ServerSetDesiredGait(const FGameplayTag& NewDesiredGait);
	virtual void ServerSetDesiredGait_Implementation(const FGameplayTag& NewDesiredGait);


	// Gait

public:
	const FGameplayTag& GetGait() const;

private:
	void SetGait(const FGameplayTag& NewGait);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnGaitChanged(const FGameplayTag& PreviousGait);
	virtual void OnGaitChanged_Implementation(const FGameplayTag& PreviousGait);

private:
	void RefreshGait();

	FGameplayTag CalculateMaxAllowedGait() const;

	FGameplayTag CalculateActualGait(const FGameplayTag& MaxAllowedGait) const;

	bool CanSprint() const;

	// Overlay Mode

public:
	const FGameplayTag& GetOverlayMode() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character", Meta = (AutoCreateRefTerm = "NewOverlayMode"))
	void SetOverlayMode(UPARAM(meta=(Categories="Als.OverlayMode")) const FGameplayTag& NewOverlayMode);

private:
	UFUNCTION(Server, Reliable)
	void ServerSetOverlayMode(const FGameplayTag& NewOverlayMode);
	virtual void ServerSetOverlayMode_Implementation(const FGameplayTag& NewOverlayMode);


	UFUNCTION()
	void OnReplicated_OverlayMode(const FGameplayTag& PreviousOverlayMode);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnOverlayModeChanged(const FGameplayTag& PreviousOverlayMode);
	virtual void OnOverlayModeChanged_Implementation(const FGameplayTag& PreviousOverlayMode);

	// Locomotion Action

public:
	const FGameplayTag& GetLocomotionAction() const;

	void SetLocomotionAction(const FGameplayTag& NewLocomotionAction);

	void NotifyLocomotionActionChanged(const FGameplayTag& PreviousLocomotionAction);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnLocomotionActionChanged(const FGameplayTag& PreviousLocomotionAction);
	virtual void OnLocomotionActionChanged_Implementation(const FGameplayTag& PreviousLocomotionAction);

	// View

public:
	// override for owner
	virtual FRotator GetViewRotation() const;

private:
	void SetRawViewRotation(const FRotator& NewViewRotation);

	UFUNCTION(Server, Unreliable)
	void ServerSetRawViewRotation(const FRotator& NewViewRotation);
	virtual void ServerSetRawViewRotation_Implementation(const FRotator& NewViewRotation);


	UFUNCTION()
	void OnReplicated_RawViewRotation();

public:
	void CorrectViewNetworkSmoothing(const FRotator& NewViewRotation);

public:
	const FAlsViewState& GetViewState() const;

private:
	void RefreshView(float DeltaTime);

	void RefreshViewNetworkSmoothing(float DeltaTime);

	// Locomotion

public:
	const FVector& GetInputDirection() const;

	const FAlsLocomotionState& GetLocomotionState() const;

private:
	void SetInputDirection(FVector NewInputDirection);

	void RefreshLocomotionLocationAndRotation(float DeltaTime);

	void RefreshLocomotion(float DeltaTime);

	// Jumping

public:
	// call from owner Character
	UFUNCTION(BlueprintPure, Category="Als Character")
	virtual bool CanJump();

	// call from owner Character
	UFUNCTION(BlueprintCallable, Category="Als Character")
	virtual void OnJumped();

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnJumpedNetworked();
	virtual void MulticastOnJumpedNetworked_Implementation();

	void OnJumpedNetworked();

	// Rotation

public:
	void CharacterMovement_OnPhysicsRotation(float DeltaTime);

private:
	void RefreshGroundedRotation(float DeltaTime);

protected:
	virtual bool RefreshCustomGroundedMovingRotation(float DeltaTime);

	virtual bool RefreshCustomGroundedNotMovingRotation(float DeltaTime);

	void RefreshGroundedMovingAimingRotation(float DeltaTime);

	void RefreshGroundedNotMovingAimingRotation(float DeltaTime);

	float CalculateRotationInterpolationSpeed() const;

private:
	void ApplyRotationYawSpeed(float DeltaTime);

	void RefreshInAirRotation(float DeltaTime);

protected:
	virtual bool RefreshCustomInAirRotation(float DeltaTime);

	void RefreshInAirAimingRotation(float DeltaTime);

	void RefreshRotation(float TargetYawAngle, float DeltaTime, float RotationInterpolationSpeed);

	void RefreshRotationExtraSmooth(float TargetYawAngle, float DeltaTime,
	                                float RotationInterpolationSpeed, float TargetYawAngleRotationSpeed);

	void RefreshRotationInstant(float TargetYawAngle, ETeleportType Teleport = ETeleportType::None);

	void RefreshTargetYawAngleUsingLocomotionRotation();

	void RefreshTargetYawAngle(float TargetYawAngle);

	void RefreshViewRelativeTargetYawAngle();

	// Rotation Lock

public:
	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character")
	void LockRotation(float TargetYawAngle);

	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character")
	void UnLockRotation();

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastLockRotation(float TargetYawAngle);
	virtual void MulticastLockRotation_Implementation(float TargetYawAngle);


	UFUNCTION(NetMulticast, Reliable)
	void MulticastUnLockRotation();
	virtual void MulticastUnLockRotation_Implementation();


	// Rolling

public:
	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character")
	void TryStartRolling(float PlayRate = 1.0f);

	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	UAnimMontage* SelectRollMontage();
	virtual UAnimMontage* SelectRollMontage_Implementation();

	bool IsRollingAllowedToStart(const UAnimMontage* Montage) const;

private:
	void StartRolling(float PlayRate, float TargetYawAngle);

	UFUNCTION(Server, Reliable)
	void ServerStartRolling(UAnimMontage* Montage, float PlayRate, float StartYawAngle, float TargetYawAngle);
	virtual void ServerStartRolling_Implementation(UAnimMontage* Montage, float PlayRate, float StartYawAngle, float TargetYawAngle);


	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartRolling(UAnimMontage* Montage, float PlayRate, float StartYawAngle, float TargetYawAngle);
	virtual void MulticastStartRolling_Implementation(UAnimMontage* Montage, float PlayRate, float StartYawAngle, float TargetYawAngle);
	
	void StartRollingImplementation(UAnimMontage* Montage, float PlayRate, float StartYawAngle, float TargetYawAngle);

	void RefreshRolling(float DeltaTime);

	void RefreshRollingPhysics(float DeltaTime);

	// Mantling

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	bool IsMantlingAllowedToStart() const;
	virtual bool IsMantlingAllowedToStart_Implementation() const;


	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character")
	bool TryStartMantlingGrounded();

private:
	bool TryStartMantlingInAir();

	bool TryStartMantling(const FAlsMantlingTraceSettings& TraceSettings);

	UFUNCTION(Server, Reliable)
	void ServerStartMantling(const FAlsMantlingParameters& Parameters);
	virtual void ServerStartMantling_Implementation(const FAlsMantlingParameters& Parameters);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartMantling(const FAlsMantlingParameters& Parameters);
	virtual void MulticastStartMantling_Implementation(const FAlsMantlingParameters& Parameters);
	
	void StartMantlingImplementation(const FAlsMantlingParameters& Parameters);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	UAlsMantlingSettings* SelectMantlingSettings(EAlsMantlingType MantlingType);
	virtual UAlsMantlingSettings* SelectMantlingSettings_Implementation(EAlsMantlingType MantlingType);

	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnMantlingStarted(const FAlsMantlingParameters& Parameters);
	virtual void OnMantlingStarted_Implementation(const FAlsMantlingParameters& Parameters);

private:
	void RefreshMantling();

	void StopMantling();

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnMantlingEnded();
	virtual void OnMantlingEnded_Implementation();

	// Ragdolling

public:
	bool IsRagdollingAllowedToStart() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character")
	void StartRagdolling();

private:
	UFUNCTION(Server, Reliable)
	void ServerStartRagdolling();
	virtual void ServerStartRagdolling_Implementation();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartRagdolling();
	virtual void MulticastStartRagdolling_Implementation();


	void StartRagdollingImplementation();

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnRagdollingStarted();
	virtual void OnRagdollingStarted_Implementation();

public:
	bool IsRagdollingAllowedToStop() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Als Character")
	bool TryStopRagdolling();

private:
	UFUNCTION(Server, Reliable)
	void ServerStopRagdolling();
	virtual void ServerStopRagdolling_Implementation();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopRagdolling();
	virtual void MulticastStopRagdolling_Implementation();

	void StopRagdollingImplementation();

public:
	void FinalizeRagdolling();

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	UAnimMontage* SelectGetUpMontage(bool bRagdollFacedUpward);
	virtual UAnimMontage* SelectGetUpMontage_Implementation(bool bRagdollFacedUpward);


	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnRagdollingEnded();
	virtual void OnRagdollingEnded_Implementation();


private:
	void SetRagdollTargetLocation(const FVector& NewTargetLocation);

	UFUNCTION(Server, Unreliable)
	void ServerSetRagdollTargetLocation(const FVector_NetQuantize100& NewTargetLocation);
	virtual void ServerSetRagdollTargetLocation_Implementation(const FVector_NetQuantize100& NewTargetLocation);


	void RefreshRagdolling(float DeltaTime);

	void RefreshRagdollingActorTransform(float DeltaTime);

	// Debug

public:
	// call from owner
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& Unused, float& VerticalLocation);

private:
	static void DisplayDebugHeader(const UCanvas* Canvas, const FText& HeaderText, const FLinearColor& HeaderColor,
	                               float Scale, float HorizontalLocation, float& VerticalLocation);

	void DisplayDebugCurves(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	void DisplayDebugState(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	void DisplayDebugShapes(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	void DisplayDebugTraces(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	void DisplayDebugMantling(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;


#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) override;
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif
	
	
};

inline bool UAlsComponent::IsSimulatedProxyTeleported() const
{
	return bSimulatedProxyTeleported;
}

inline const FGameplayTag& UAlsComponent::GetViewMode() const
{
	return ViewMode;
}

inline const FGameplayTag& UAlsComponent::GetLocomotionMode() const
{
	return LocomotionMode;
}

inline bool UAlsComponent::IsDesiredAiming() const
{
	return bDesiredAiming;
}

inline const FGameplayTag& UAlsComponent::GetDesiredRotationMode() const
{
	return DesiredRotationMode;
}

inline const FGameplayTag& UAlsComponent::GetRotationMode() const
{
	return RotationMode;
}

inline const FGameplayTag& UAlsComponent::GetDesiredStance() const
{
	return DesiredStance;
}

inline const FGameplayTag& UAlsComponent::GetStance() const
{
	return Stance;
}

inline const FGameplayTag& UAlsComponent::GetDesiredGait() const
{
	return DesiredGait;
}

inline const FGameplayTag& UAlsComponent::GetGait() const
{
	return Gait;
}

inline const FGameplayTag& UAlsComponent::GetOverlayMode() const
{
	return OverlayMode;
}

inline const FGameplayTag& UAlsComponent::GetLocomotionAction() const
{
	return LocomotionAction;
}

inline const FVector& UAlsComponent::GetInputDirection() const
{
	return InputDirection;
}

inline const FAlsViewState& UAlsComponent::GetViewState() const
{
	return ViewState;
}

inline const FAlsLocomotionState& UAlsComponent::GetLocomotionState() const
{
	return LocomotionState;
}

