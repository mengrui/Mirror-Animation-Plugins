// Copyright Terry Meng 2022 All Rights Reserved.

#include "MirrorAnimationBPLibrary.h"
#include "MirrorAnimation.h"
#include "BoneContainer.h"

UMirrorAnimationBPLibrary::UMirrorAnimationBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

static void GetLocalMirrorPose(
	const UAnimSequence* InSequence,
	const FBoneContainer& BoneContainer,
	float Time, FCompactPose& Pose,
	const UMirrorDataTable& MirrorDataTable)
{
	USkeleton* Skeleton = InSequence->GetSkeleton();
	FReferenceSkeleton RefSkel = Skeleton->GetReferenceSkeleton();
	Pose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(Pose, Curve, Attributes);

	FAnimExtractContext Context(Time, false);
	InSequence->GetBonePose(AnimationPoseData, Context, false);

	FAnimationRuntime::MirrorPose(Pose, MirrorDataTable);
}

void UMirrorAnimationBPLibrary::MakeMirrorAnimation(const UAnimSequence* InAnimation, const UMirrorDataTable* MirrorDataTable)
{
	if (InAnimation == nullptr || MirrorDataTable == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimationMirrorModifier failed. Reason: Invalid Animation"));
		return;
	}

	if (MirrorDataTable == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimationMirrorModifier failed. Reason: Invalid MirrorDataTable"));
		return;
	}

	FString PackagePath = InAnimation->GetPackage()->GetPathName() + FString(TEXT("_Mirror"));
	UPackage* Package = CreatePackage(*PackagePath);
	FString ObjName = InAnimation->GetName() + FString(TEXT("_Mirror"));
	//UAnimSequence* Animation = NewObject<UAnimSequence>(Package, *ObjName);
	UAnimSequence* Animation = DuplicateObject<UAnimSequence>(InAnimation, Package, *ObjName);
	FString const PackageName = Package->GetName();
	FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	USkeleton* Skeleton = Animation->GetSkeleton();
	if (Skeleton == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimationMirrorModifier failed. Reason: Animation with invalid Skeleton. Animation: %s"),
			*GetNameSafe(Animation));
		return;
	}

	const auto& RefSkeleton = Skeleton->GetReferenceSkeleton();
	TArray<FBoneIndexType> RequiredBones;
	for (auto i = 0; i < RefSkeleton.GetNum(); i++)
	{
		RequiredBones.Add(i);
	}
	RefSkeleton.EnsureParentsExistAndSort(RequiredBones);

	FMemMark Mark(FMemStack::Get());
	FBoneContainer BoneContainer(RequiredBones, false, *Skeleton);

	const float AnimLength = Animation->GetPlayLength();
	const float SampleInterval = Animation->GetSamplingFrameRate().AsInterval();
	int32 FrameNumber = Animation->GetNumberOfFrames();

	IAnimationDataController& Controller = Animation->GetController();
	for (auto BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); BoneIndex++)
	{
		FRawAnimSequenceTrack RawTrack;
		for (auto FrameIndex = 0; FrameIndex < FrameNumber; FrameIndex++)
		{
			FCompactPose Pose;
			GetLocalMirrorPose(InAnimation, BoneContainer, InAnimation->GetTimeAtFrame(FrameIndex), Pose, *MirrorDataTable);
			const FCompactPoseBoneIndex CompactPoseBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneIndex));
			FTransform LocalTransform = Pose[CompactPoseBoneIndex];

			RawTrack.ScaleKeys.Add(FVector3f(LocalTransform.GetScale3D()));
			RawTrack.PosKeys.Add(FVector3f(LocalTransform.GetTranslation()));
			RawTrack.RotKeys.Add(FQuat4f(LocalTransform.GetRotation()));
		}
		Controller.AddBoneTrack(RefSkeleton.GetBoneName(BoneIndex));
		Controller.SetBoneTrackKeys(RefSkeleton.GetBoneName(BoneIndex), RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys);
	}

	UPackage::SavePackage(Package, NULL, RF_Standalone, *PackageFileName, GError, nullptr, false, true, SAVE_NoError);
}

static float GetGaussianWeight(float Dist, float Strength)
{
	// from https://en.wikipedia.org/wiki/Gaussian_blur
	float Strength2 = Strength * Strength;
	return (1.0f / FMath::Sqrt(2 * PI * Strength2)) * FMath::Exp(-(Dist * Dist) / (2 * Strength2));
}

static TArray<float> GetGaussianWeights(int32 Num)
{
	TArray<float> Weights;
	float total = 0;
	for (int32 i = -Num; i <= Num; i++)
	{
		float weight = GetGaussianWeight(0.1f * i, 0.1f * Num);
		Weights.Add(weight);
		total += weight;
	}

	for (int i = 0; i < Weights.Num(); i++)
	{
		Weights[i] /= total;
	}

	return Weights;
}