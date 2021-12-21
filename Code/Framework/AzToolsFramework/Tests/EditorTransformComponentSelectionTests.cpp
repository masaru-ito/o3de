/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/IntersectSegment.h>
#include <AzCore/Math/ToString.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Entity/EntityContext.h>
#include <AzFramework/Viewport/ViewportScreen.h>
#include <AzManipulatorTestFramework/AzManipulatorTestFramework.h>
#include <AzManipulatorTestFramework/AzManipulatorTestFrameworkTestHelpers.h>
#include <AzManipulatorTestFramework/AzManipulatorTestFrameworkUtils.h>
#include <AzManipulatorTestFramework/ImmediateModeActionDispatcher.h>
#include <AzManipulatorTestFramework/IndirectManipulatorViewportInteraction.h>
#include <AzManipulatorTestFramework/ViewportInteraction.h>
#include <AzQtComponents/Components/GlobalEventFilter.h>
#include <AzTest/AzTest.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/Entity/EditorEntityActionComponent.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>
#include <AzToolsFramework/Entity/EditorEntityModel.h>
#include <AzToolsFramework/ToolsComponents/EditorLockComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityComponent.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>
#include <AzToolsFramework/Viewport/ActionBus.h>
#include <AzToolsFramework/ViewportSelection/EditorDefaultSelection.h>
#include <AzToolsFramework/ViewportSelection/EditorInteractionSystemViewportSelectionRequestBus.h>
#include <AzToolsFramework/ViewportSelection/EditorPickEntitySelection.h>
#include <AzToolsFramework/ViewportSelection/EditorSelectionUtil.h>
#include <AzToolsFramework/ViewportSelection/EditorTransformComponentSelection.h>
#include <AzToolsFramework/ViewportSelection/EditorVisibleEntityDataCache.h>
#include <AzToolsFramework/ViewportUi/ViewportUiManager.h>

#include <Tests/BoundsTestComponent.h>

namespace UnitTest
{
    using AzToolsFramework::ViewportInteraction::BuildMouseButtons;
    using AzToolsFramework::ViewportInteraction::BuildMouseInteraction;
    using AzToolsFramework::ViewportInteraction::BuildMousePick;

    AzToolsFramework::EntityIdList SelectedEntities()
    {
        AzToolsFramework::EntityIdList selectedEntitiesBefore;
        AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(
            selectedEntitiesBefore, &AzToolsFramework::ToolsApplicationRequestBus::Events::GetSelectedEntities);
        return selectedEntitiesBefore;
    }

    class EditorEntityVisibilityCacheFixture : public ToolsApplicationFixture
    {
    public:
        void CreateLayerAndEntityHierarchy()
        {
            // Set up entity layer hierarchy.
            const AZ::EntityId a = CreateDefaultEditorEntity("A");
            const AZ::EntityId b = CreateDefaultEditorEntity("B");
            const AZ::EntityId c = CreateDefaultEditorEntity("C");

            m_layerId = CreateEditorLayerEntity("Layer");

            AZ::TransformBus::Event(a, &AZ::TransformBus::Events::SetParent, m_layerId);
            AZ::TransformBus::Event(b, &AZ::TransformBus::Events::SetParent, a);
            AZ::TransformBus::Event(c, &AZ::TransformBus::Events::SetParent, b);

            // Add entity ids we want to track, to the visibility cache.
            m_entityIds.insert(m_entityIds.begin(), { a, b, c });
            m_cache.AddEntityIds(m_entityIds);
        }

        AzToolsFramework::EntityIdList m_entityIds;
        AZ::EntityId m_layerId;
        AzToolsFramework::EditorVisibleEntityDataCache m_cache;
    };

    TEST_F(EditorEntityVisibilityCacheFixture, LayerLockAffectsChildEntitiesInEditorEntityCache)
    {
        // Given
        CreateLayerAndEntityHierarchy();

        // Check preconditions.
        EXPECT_FALSE(m_cache.IsVisibleEntityLocked(m_cache.GetVisibleEntityIndexFromId(m_entityIds[0]).value()));
        EXPECT_FALSE(m_cache.IsVisibleEntityLocked(m_cache.GetVisibleEntityIndexFromId(m_entityIds[1]).value()));
        EXPECT_FALSE(m_cache.IsVisibleEntityLocked(m_cache.GetVisibleEntityIndexFromId(m_entityIds[2]).value()));

        // When
        AzToolsFramework::SetEntityLockState(m_layerId, true);

        // Then
        EXPECT_TRUE(m_cache.IsVisibleEntityLocked(m_cache.GetVisibleEntityIndexFromId(m_entityIds[0]).value()));
        EXPECT_TRUE(m_cache.IsVisibleEntityLocked(m_cache.GetVisibleEntityIndexFromId(m_entityIds[1]).value()));
        EXPECT_TRUE(m_cache.IsVisibleEntityLocked(m_cache.GetVisibleEntityIndexFromId(m_entityIds[2]).value()));
    }

    TEST_F(EditorEntityVisibilityCacheFixture, LayerVisibilityAffectsChildEntitiesInEditorEntityCache)
    {
        // Given
        CreateLayerAndEntityHierarchy();

        // Check preconditions.
        EXPECT_TRUE(m_cache.IsVisibleEntityVisible(m_cache.GetVisibleEntityIndexFromId(m_entityIds[0]).value()));
        EXPECT_TRUE(m_cache.IsVisibleEntityVisible(m_cache.GetVisibleEntityIndexFromId(m_entityIds[1]).value()));
        EXPECT_TRUE(m_cache.IsVisibleEntityVisible(m_cache.GetVisibleEntityIndexFromId(m_entityIds[2]).value()));

        // When
        AzToolsFramework::SetEntityVisibility(m_layerId, false);

        // Then
        EXPECT_FALSE(m_cache.IsVisibleEntityVisible(m_cache.GetVisibleEntityIndexFromId(m_entityIds[0]).value()));
        EXPECT_FALSE(m_cache.IsVisibleEntityVisible(m_cache.GetVisibleEntityIndexFromId(m_entityIds[1]).value()));
        EXPECT_FALSE(m_cache.IsVisibleEntityVisible(m_cache.GetVisibleEntityIndexFromId(m_entityIds[2]).value()));
    }

    // Fixture to support testing EditorTransformComponentSelection functionality on an Entity selection.
    class EditorTransformComponentSelectionFixture : public ToolsApplicationFixture
    {
    public:
        void SetUpEditorFixtureImpl() override
        {
            m_entityId1 = CreateDefaultEditorEntity("Entity1");
            m_entityIds.push_back(m_entityId1);
        }

    public:
        AZ::EntityId m_entityId1;
        AzToolsFramework::EntityIdList m_entityIds;
    };

    AZ::EntityId CreateEntityWithBounds(const char* entityName)
    {
        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity(entityName, &entity);

        entity->Deactivate();
        entity->CreateComponent<BoundsTestComponent>();
        entity->Activate();

        return entityId;
    }

    class EditorTransformComponentSelectionViewportPickingFixture : public ToolsApplicationFixture
    {
    public:
        void SetUpEditorFixtureImpl() override
        {
            auto* app = GetApplication();
            // register a simple component implementing BoundsRequestBus and EditorComponentSelectionRequestsBus
            app->RegisterComponentDescriptor(BoundsTestComponent::CreateDescriptor());

            m_entityId1 = CreateEntityWithBounds("Entity1");
            m_entityId2 = CreateEntityWithBounds("Entity2");
            m_entityId3 = CreateEntityWithBounds("Entity3");
        }

        void PositionEntities()
        {
            // the initial starting position of the entities
            AZ::TransformBus::Event(
                m_entityId1, &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(m_entity1WorldTranslation));
            AZ::TransformBus::Event(
                m_entityId2, &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(m_entity2WorldTranslation));
            AZ::TransformBus::Event(
                m_entityId3, &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(m_entity3WorldTranslation));
        }

        static void PositionCamera(AzFramework::CameraState& cameraState)
        {
            // initial camera position (looking down the negative x-axis)
            AzFramework::SetCameraTransform(
                cameraState,
                AZ::Transform::CreateFromQuaternionAndTranslation(
                    AZ::Quaternion::CreateFromEulerAnglesDegrees(AZ::Vector3(0.0f, 0.0f, 90.0f)), AZ::Vector3(10.0f, 15.0f, 10.0f)));
        }

        AZ::EntityId m_entityId1;
        AZ::EntityId m_entityId2;
        AZ::EntityId m_entityId3;
        AZ::Vector3 m_entity1WorldTranslation = AZ::Vector3(5.0f, 15.0f, 10.0f);
        AZ::Vector3 m_entity2WorldTranslation = AZ::Vector3(5.0f, 14.0f, 10.0f);
        AZ::Vector3 m_entity3WorldTranslation = AZ::Vector3(5.0f, 16.0f, 10.0f);
    };

    void ArrangeIndividualRotatedEntitySelection(const AzToolsFramework::EntityIdList& entityIds, const AZ::Quaternion& orientation)
    {
        for (auto entityId : entityIds)
        {
            AZ::TransformBus::Event(entityId, &AZ::TransformBus::Events::SetLocalRotationQuaternion, orientation);
        }
    }

    AZStd::optional<AZ::Transform> GetManipulatorTransform()
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        AZStd::optional<AZ::Transform> manipulatorTransform;
        EditorTransformComponentSelectionRequestBus::EventResult(
            manipulatorTransform, AzToolsFramework::GetEntityContextId(),
            &EditorTransformComponentSelectionRequestBus::Events::GetManipulatorTransform);
        return manipulatorTransform;
    }

    void RefreshManipulators(const AzToolsFramework::EditorTransformComponentSelectionRequestBus::Events::RefreshType refreshType)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        EditorTransformComponentSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(), &EditorTransformComponentSelectionRequestBus::Events::RefreshManipulators, refreshType);
    }

    void SetTransformMode(const AzToolsFramework::EditorTransformComponentSelectionRequestBus::Events::Mode transformMode)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        EditorTransformComponentSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(), &EditorTransformComponentSelectionRequestBus::Events::SetTransformMode, transformMode);
    }

    void OverrideManipulatorOrientation(const AZ::Quaternion& orientation)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        EditorTransformComponentSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(), &EditorTransformComponentSelectionRequestBus::Events::OverrideManipulatorOrientation,
            orientation);
    }

    void OverrideManipulatorTranslation(const AZ::Vector3& translation)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        EditorTransformComponentSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(), &EditorTransformComponentSelectionRequestBus::Events::OverrideManipulatorTranslation,
            translation);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // EditorTransformComponentSelection Tests

    TEST_F(EditorTransformComponentSelectionFixture, FocusIsNotChangedWhileSwitchingViewportInteractionRequestInstance)
    {
        // setup a dummy widget and make it the active window to ensure focus in/out events are fired
        auto dummyWidget = AZStd::make_unique<QWidget>();
        QApplication::setActiveWindow(dummyWidget.get());

        // note: it is important to make sure the focus widget is parented to the dummy widget to have focus in/out events fire
        auto focusWidget = AZStd::make_unique<UnitTest::FocusInteractionWidget>(dummyWidget.get());

        const auto previousFocusWidget = QApplication::focusWidget();

        // Given
        // setup viewport ui system
        AzToolsFramework::ViewportUi::ViewportUiManager viewportUiManager;
        viewportUiManager.ConnectViewportUiBus(AzToolsFramework::ViewportUi::DefaultViewportId);
        viewportUiManager.InitializeViewportUi(&m_editorActions.m_defaultWidget, focusWidget.get());

        // begin EditorPickEntitySelection
        using AzToolsFramework::EditorInteractionSystemViewportSelectionRequestBus;
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(), &EditorInteractionSystemViewportSelectionRequestBus::Events::SetHandler,
            [](const AzToolsFramework::EditorVisibleEntityDataCache* entityDataCache,
               [[maybe_unused]] AzToolsFramework::ViewportEditorModeTrackerInterface* viewportEditorModeTracker)
            {
                return AZStd::make_unique<AzToolsFramework::EditorPickEntitySelection>(entityDataCache, viewportEditorModeTracker);
            });

        // When
        // a mouse event is sent to the focus widget (set to be the render overlay in the viewport ui system)
        QTest::mouseClick(focusWidget.get(), Qt::MouseButton::LeftButton);

        // Then
        // focus should not change
        EXPECT_FALSE(focusWidget->hasFocus());
        EXPECT_EQ(previousFocusWidget, QApplication::focusWidget());

        // clean up
        viewportUiManager.DisconnectViewportUiBus();
        focusWidget.reset();
        dummyWidget.reset();
    }

    TEST_F(EditorTransformComponentSelectionFixture, ManipulatorOrientationIsResetWhenEntityOrientationIsReset)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        AzToolsFramework::SelectEntity(m_entityId1);

        ArrangeIndividualRotatedEntitySelection(m_entityIds, AZ::Quaternion::CreateRotationX(AZ::DegToRad(90.0f)));
        RefreshManipulators(EditorTransformComponentSelectionRequestBus::Events::RefreshType::All);

        SetTransformMode(EditorTransformComponentSelectionRequestBus::Events::Mode::Rotation);

        const AZ::Transform manipulatorTransformBefore = GetManipulatorTransform().value_or(AZ::Transform::CreateIdentity());

        // check preconditions - manipulator transform matches parent/world transform (identity)
        EXPECT_THAT(manipulatorTransformBefore.GetBasisY(), IsClose(AZ::Vector3::CreateAxisY()));
        EXPECT_THAT(manipulatorTransformBefore.GetBasisZ(), IsClose(AZ::Vector3::CreateAxisZ()));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // R - reset entity and manipulator orientation when in Rotation Mode
        QTest::keyPress(&m_editorActions.m_defaultWidget, Qt::Key_R);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        const AZ::Transform manipulatorTransformAfter = GetManipulatorTransform().value_or(AZ::Transform::CreateIdentity());

        // check postconditions - manipulator transform matches parent/world transform (identity)
        EXPECT_THAT(manipulatorTransformAfter.GetBasisY(), IsClose(AZ::Vector3::CreateAxisY()));
        EXPECT_THAT(manipulatorTransformAfter.GetBasisZ(), IsClose(AZ::Vector3::CreateAxisZ()));

        for (auto entityId : m_entityIds)
        {
            // create invalid starting orientation to guarantee correct data is coming from GetLocalRotationQuaternion
            AZ::Quaternion entityOrientation = AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisX(), 90.0f);
            AZ::TransformBus::EventResult(entityOrientation, entityId, &AZ::TransformBus::Events::GetLocalRotationQuaternion);

            // manipulator orientation matches entity orientation
            EXPECT_THAT(entityOrientation, IsClose(manipulatorTransformAfter.GetRotation()));
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(EditorTransformComponentSelectionFixture, EntityOrientationRemainsConstantWhenOnlyManipulatorOrientationIsReset)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        AzToolsFramework::SelectEntity(m_entityId1);

        const AZ::Quaternion initialEntityOrientation = AZ::Quaternion::CreateRotationX(AZ::DegToRad(90.0f));
        ArrangeIndividualRotatedEntitySelection(m_entityIds, initialEntityOrientation);

        // assign new orientation to manipulator which does not match entity orientation
        OverrideManipulatorOrientation(AZ::Quaternion::CreateRotationZ(AZ::DegToRad(90.0f)));

        SetTransformMode(EditorTransformComponentSelectionRequestBus::Events::Mode::Rotation);

        const AZ::Transform manipulatorTransformBefore = GetManipulatorTransform().value_or(AZ::Transform::CreateIdentity());

        // check preconditions - manipulator transform matches manipulator orientation override (not entity transform)
        EXPECT_THAT(manipulatorTransformBefore.GetBasisX(), IsClose(AZ::Vector3::CreateAxisY()));
        EXPECT_THAT(manipulatorTransformBefore.GetBasisY(), IsClose(-AZ::Vector3::CreateAxisX()));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // Ctrl+R - reset only manipulator orientation when in Rotation Mode
        QTest::keyPress(&m_editorActions.m_defaultWidget, Qt::Key_R, Qt::ControlModifier);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        const AZ::Transform manipulatorTransformAfter = GetManipulatorTransform().value_or(AZ::Transform::CreateIdentity());

        // check postconditions - manipulator transform matches parent/world space (manipulator override was cleared)
        EXPECT_THAT(manipulatorTransformAfter.GetBasisY(), IsClose(AZ::Vector3::CreateAxisY()));
        EXPECT_THAT(manipulatorTransformAfter.GetBasisZ(), IsClose(AZ::Vector3::CreateAxisZ()));

        for (auto entityId : m_entityIds)
        {
            AZ::Quaternion entityOrientation;
            AZ::TransformBus::EventResult(entityOrientation, entityId, &AZ::TransformBus::Events::GetLocalRotationQuaternion);

            // entity transform matches initial (entity transform was not reset, only manipulator was)
            EXPECT_THAT(entityOrientation, IsClose(initialEntityOrientation));
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(EditorTransformComponentSelectionFixture, TestComponentPropertyNotificationIsSentAfterModifyingSlice)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        AUTO_RESULT_IF_SETTING_TRUE(UnitTest::prefabSystemSetting, true)

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        AZ::Entity* grandParent = nullptr;
        AZ::Entity* parent = nullptr;
        AZ::Entity* child = nullptr;

        AZ::EntityId grandParentId = CreateDefaultEditorEntity("GrandParent", &grandParent);
        AZ::EntityId parentId = CreateDefaultEditorEntity("Parent", &parent);
        AZ::EntityId childId = CreateDefaultEditorEntity("Child", &child);

        AZ::TransformBus::Event(childId, &AZ::TransformInterface::SetParent, parentId);
        AZ::TransformBus::Event(parentId, &AZ::TransformInterface::SetParent, grandParentId);

        UnitTest::SliceAssets sliceAssets;
        const auto sliceAssetId = UnitTest::SaveAsSlice({ grandParent }, GetApplication(), sliceAssets);

        AzToolsFramework::EntityList instantiatedEntities = UnitTest::InstantiateSlice(sliceAssetId, sliceAssets);

        const AZ::EntityId entityIdToMove = instantiatedEntities.back()->GetId();
        EditorEntityComponentChangeDetector editorEntityChangeDetector(entityIdToMove);

        AzToolsFramework::SelectEntity(entityIdToMove);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        EditorTransformComponentSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(),
            &EditorTransformComponentSelectionRequestBus::Events::CopyOrientationToSelectedEntitiesIndividual,
            AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisX(), AZ::DegToRad(90.0f)));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(editorEntityChangeDetector.ChangeDetected());
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        UnitTest::DestroySlices(sliceAssets);
    }

    TEST_F(EditorTransformComponentSelectionFixture, CopyOrientationToSelectedEntitiesIndividualDoesNotAffectScale)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;
        using ::testing::FloatNear;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        const auto expectedRotation = AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisZ(), AZ::DegToRad(45.0f));

        AZ::TransformBus::Event(m_entityId1, &AZ::TransformBus::Events::SetWorldTranslation, AZ::Vector3::CreateAxisX(10.0f));
        AZ::TransformBus::Event(m_entityId1, &AZ::TransformBus::Events::SetLocalUniformScale, 2.0f);
        AZ::TransformBus::Event(m_entityId1, &AZ::TransformBus::Events::SetLocalRotationQuaternion, expectedRotation);

        AzToolsFramework::SelectEntity(m_entityId1);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        EditorTransformComponentSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(),
            &EditorTransformComponentSelectionRequestBus::Events::CopyOrientationToSelectedEntitiesIndividual, expectedRotation);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        float scale = 0.0f;
        AZ::Quaternion rotation = AZ::Quaternion::CreateIdentity();

        AZ::TransformBus::EventResult(rotation, m_entityId1, &AZ::TransformBus::Events::GetLocalRotationQuaternion);
        AZ::TransformBus::EventResult(scale, m_entityId1, &AZ::TransformBus::Events::GetLocalUniformScale);

        EXPECT_THAT(rotation, IsClose(expectedRotation));
        EXPECT_THAT(scale, FloatNear(2.0f, 0.001f));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(EditorTransformComponentSelectionFixture, InvertSelectionIgnoresLockedAndHiddenEntities)
    {
        using ::testing::UnorderedElementsAreArray;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        // note: entity1 is created in the fixture setup
        AzToolsFramework::SelectEntity(m_entityId1);

        AZ::EntityId entity2 = CreateDefaultEditorEntity("Entity2");
        AZ::EntityId entity3 = CreateDefaultEditorEntity("Entity3");
        AZ::EntityId entity4 = CreateDefaultEditorEntity("Entity4");
        AZ::EntityId entity5 = CreateDefaultEditorEntity("Entity5");
        AZ::EntityId entity6 = CreateDefaultEditorEntity("Entity6");

        AzToolsFramework::SetEntityVisibility(entity2, false);
        AzToolsFramework::SetEntityLockState(entity3, true);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // 'Invert Selection' shortcut
        QTest::keyPress(&m_editorActions.m_defaultWidget, Qt::Key_I, Qt::ControlModifier | Qt::ShiftModifier);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        const AzToolsFramework::EntityIdList selectedEntities = SelectedEntities();
        const AzToolsFramework::EntityIdList expectedSelectedEntities = { entity4, entity5, entity6 };
        EXPECT_THAT(selectedEntities, UnorderedElementsAreArray(expectedSelectedEntities));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(EditorTransformComponentSelectionFixture, SelectAllIgnoresLockedAndHiddenEntities)
    {
        using ::testing::UnorderedElementsAreArray;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        AZ::EntityId entity2 = CreateDefaultEditorEntity("Entity2");
        AZ::EntityId entity3 = CreateDefaultEditorEntity("Entity3");
        AZ::EntityId entity4 = CreateDefaultEditorEntity("Entity4");
        AZ::EntityId entity5 = CreateDefaultEditorEntity("Entity5");
        AZ::EntityId entity6 = CreateDefaultEditorEntity("Entity6");

        AzToolsFramework::SetEntityVisibility(entity5, false);
        AzToolsFramework::SetEntityLockState(entity6, true);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // 'Select All' shortcut
        QTest::keyPress(&m_editorActions.m_defaultWidget, Qt::Key_A, Qt::ControlModifier);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        const AzToolsFramework::EntityIdList selectedEntities = SelectedEntities();
        const AzToolsFramework::EntityIdList expectedSelectedEntities = { m_entityId1, entity2, entity3, entity4 };
        EXPECT_THAT(selectedEntities, UnorderedElementsAreArray(expectedSelectedEntities));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // fixture for use with the indirect manipulator test framework
    using EditorTransformComponentSelectionViewportPickingManipulatorTestFixture =
        IndirectCallManipulatorViewportInteractionFixtureMixin<EditorTransformComponentSelectionViewportPickingFixture>;

    TEST_F(EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, StickySingleClickWithNoSelectionWillSelectEntity)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        using ::testing::Eq;
        auto selectedEntitiesBefore = SelectedEntities();
        EXPECT_TRUE(selectedEntitiesBefore.empty());

        // calculate the position in screen space of the initial entity position
        const auto entity1ScreenPosition = AzFramework::WorldToScreen(m_entity1WorldTranslation, m_cameraState);

        // click the entity in the viewport
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(entity1ScreenPosition)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity is selected
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter.size(), Eq(1));
        EXPECT_THAT(selectedEntitiesAfter.front(), Eq(m_entityId1));
    }

    TEST_F(EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, UnstickySingleClickWithNoSelectionWillSelectEntity)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        using ::testing::Eq;
        auto selectedEntitiesBefore = SelectedEntities();
        EXPECT_TRUE(selectedEntitiesBefore.empty());

        // calculate the position in screen space of the initial entity position
        const auto entity1ScreenPosition = AzFramework::WorldToScreen(m_entity1WorldTranslation, m_cameraState);

        // click the entity in the viewport
        m_actionDispatcher->SetStickySelect(false)
            ->CameraState(m_cameraState)
            ->MousePosition(entity1ScreenPosition)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity is selected
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter.size(), Eq(1));
        EXPECT_THAT(selectedEntitiesAfter.front(), Eq(m_entityId1));
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture,
        StickySingleClickOffEntityWithSelectionWillNotDeselectEntity)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        // position in space above the entities
        const auto clickOffPositionWorld = AZ::Vector3(5.0f, 15.0f, 12.0f);

        AzToolsFramework::SelectEntity(m_entityId1);

        // calculate the screen space position of the click
        const auto clickOffPositionScreen = AzFramework::WorldToScreen(clickOffPositionWorld, m_cameraState);

        // click the empty space in the viewport
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(clickOffPositionScreen)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity was not deselected
        using ::testing::Eq;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter.size(), Eq(1));
        EXPECT_THAT(selectedEntitiesAfter.front(), Eq(m_entityId1));
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, UnstickySingleClickOffEntityWithSelectionWillDeselectEntity)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntity(m_entityId1);

        // position in space above the entities
        const auto clickOffPositionWorld = AZ::Vector3(5.0f, 15.0f, 12.0f);
        // calculate the screen space position of the click
        const auto clickOffPositionScreen = AzFramework::WorldToScreen(clickOffPositionWorld, m_cameraState);

        // click the empty space in the viewport
        m_actionDispatcher->SetStickySelect(false)
            ->CameraState(m_cameraState)
            ->MousePosition(clickOffPositionScreen)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity was deselected
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_TRUE(selectedEntitiesAfter.empty());
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture,
        StickySingleClickOnNewEntityWithSelectionWillNotChangeSelectedEntity)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntity(m_entityId1);

        // calculate the position in screen space of the second entity
        const auto entity2ScreenPosition = AzFramework::WorldToScreen(m_entity2WorldTranslation, m_cameraState);

        // click the entity in the viewport
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity selection was not changed
        using ::testing::Eq;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter.size(), Eq(1));
        EXPECT_THAT(selectedEntitiesAfter.front(), Eq(m_entityId1));
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture,
        UnstickySingleClickOnNewEntityWithSelectionWillChangeSelectedEntity)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntity(m_entityId1);

        // calculate the position in screen space of the second entity
        const auto entity2ScreenPosition = AzFramework::WorldToScreen(m_entity2WorldTranslation, m_cameraState);

        // click the entity in the viewport
        m_actionDispatcher->SetStickySelect(false)
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity selection was changed
        using ::testing::Eq;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter.size(), Eq(1));
        EXPECT_THAT(selectedEntitiesAfter.front(), Eq(m_entityId2));
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture,
        StickyCtrlSingleClickOnNewEntityWithSelectionWillAppendSelectedEntityToSelection)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntity(m_entityId1);

        // calculate the position in screen space of the second entity
        const auto entity2ScreenPosition = AzFramework::WorldToScreen(m_entity2WorldTranslation, m_cameraState);

        // click the entity in the viewport
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Control)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity selection was changed (one entity selected to two)
        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1, m_entityId2));
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture,
        UnstickyCtrlSingleClickOnNewEntityWithSelectionWillAppendSelectedEntityToSelection)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntity(m_entityId1);

        // calculate the position in screen space of the second entity
        const auto entity2ScreenPosition = AzFramework::WorldToScreen(m_entity2WorldTranslation, m_cameraState);

        // click the entity in the viewport
        m_actionDispatcher->SetStickySelect(false)
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Control)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity selection was changed (one entity selected to two)
        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1, m_entityId2));
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture,
        StickyCtrlSingleClickOnEntityInSelectionWillRemoveEntityFromSelection)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntities({ m_entityId1, m_entityId2 });

        // calculate the position in screen space of the second entity
        const auto entity2ScreenPosition = AzFramework::WorldToScreen(m_entity2WorldTranslation, m_cameraState);

        // click the entity in the viewport
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Control)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity selection was changed (entity2 was deselected)
        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1));
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture,
        UnstickyCtrlSingleClickOnEntityInSelectionWillRemoveEntityFromSelection)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntities({ m_entityId1, m_entityId2 });

        // calculate the position in screen space of the second entity
        const auto entity2ScreenPosition = AzFramework::WorldToScreen(m_entity2WorldTranslation, m_cameraState);

        // click the entity in the viewport
        m_actionDispatcher->SetStickySelect(false)
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Control)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity selection was changed (entity2 was deselected)
        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1));
    }

    TEST_F(EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, BoxSelectWithNoInitialSelectionAddsEntitiesToSelection)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        using ::testing::Eq;
        auto selectedEntitiesBefore = SelectedEntities();
        EXPECT_THAT(selectedEntitiesBefore.size(), Eq(0));

        // calculate the position in screen space of where to begin and end the box select action
        const auto beginningPositionWorldBoxSelect = AzFramework::WorldToScreen(AZ::Vector3(5.0f, 13.5f, 10.5f), m_cameraState);
        const auto endingPositionWorldBoxSelect = AzFramework::WorldToScreen(AZ::Vector3(5.0f, 16.5f, 9.5f), m_cameraState);

        // perform a box select in the viewport
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(beginningPositionWorldBoxSelect)
            ->MouseLButtonDown()
            ->MousePosition(endingPositionWorldBoxSelect)
            ->MouseLButtonUp();

        // entities are selected
        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1, m_entityId2, m_entityId3));
    }

    TEST_F(EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, BoxSelectWithSelectionAppendsEntitiesToSelection)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntity(m_entityId1);

        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesBefore = SelectedEntities();
        EXPECT_THAT(selectedEntitiesBefore, UnorderedElementsAre(m_entityId1));

        // calculate the position in screen space of where to begin and end the box select action
        const auto beginningPositionWorldBoxSelect1 = AzFramework::WorldToScreen(AZ::Vector3(5.0f, 14.5f, 10.5f), m_cameraState);
        const auto endingPositionWorldBoxSelect1 = AzFramework::WorldToScreen(AZ::Vector3(5.0f, 13.5f, 9.5f), m_cameraState);
        const auto beginningPositionWorldBoxSelect2 = AzFramework::WorldToScreen(AZ::Vector3(5.0f, 15.5f, 10.5f), m_cameraState);
        const auto endingPositionWorldBoxSelect2 = AzFramework::WorldToScreen(AZ::Vector3(5.0f, 16.5f, 9.5f), m_cameraState);

        // perform a box select in the viewport (going left and right)
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(beginningPositionWorldBoxSelect1)
            ->MouseLButtonDown()
            ->MousePosition(endingPositionWorldBoxSelect1)
            ->MouseLButtonUp()
            ->MousePosition(beginningPositionWorldBoxSelect2)
            ->MouseLButtonDown()
            ->MousePosition(endingPositionWorldBoxSelect2)
            ->MouseLButtonUp();

        // entities are selected
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1, m_entityId2, m_entityId3));
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture,
        BoxSelectHoldingCtrlWithSelectionRemovesEntitiesFromSelection)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntities({ m_entityId1, m_entityId2, m_entityId3 });

        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesBefore = SelectedEntities();
        EXPECT_THAT(selectedEntitiesBefore, UnorderedElementsAre(m_entityId1, m_entityId2, m_entityId3));

        // calculate the position in screen space of where to begin and end the box select action
        const auto beginningPositionWorldBoxSelect = AzFramework::WorldToScreen(AZ::Vector3(5.0f, 13.5f, 10.5f), m_cameraState);
        const auto endingPositionWorldBoxSelect = AzFramework::WorldToScreen(AZ::Vector3(5.0f, 16.5f, 9.5f), m_cameraState);

        // perform a box select in the viewport
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(beginningPositionWorldBoxSelect)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Control)
            ->MouseLButtonDown()
            ->MousePosition(endingPositionWorldBoxSelect)
            ->MouseLButtonUp();

        // entities are selected
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_TRUE(selectedEntitiesAfter.empty());
    }

    TEST_F(EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, StickyDoubleClickWithSelectionWillDeselectEntities)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntities({ m_entityId1, m_entityId2, m_entityId3 });

        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesBefore = SelectedEntities();
        EXPECT_THAT(selectedEntitiesBefore, UnorderedElementsAre(m_entityId1, m_entityId2, m_entityId3));

        // position in space above the entities
        const auto clickOffPositionWorld = AZ::Vector3(5.0f, 15.0f, 12.0f);
        // calculate the screen space position of the click
        const auto clickOffPositionScreen = AzFramework::WorldToScreen(clickOffPositionWorld, m_cameraState);

        // double click to deselect entities
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(clickOffPositionScreen)
            ->MouseLButtonDoubleClick();

        // no entities are selected
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_TRUE(selectedEntitiesAfter.empty());
    }

    TEST_F(EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, UnstickyUndoOperationForChangeInSelectionIsAtomic)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntity(m_entityId1);

        // calculate the position in screen space of the second entity
        const auto entity2ScreenPosition = AzFramework::WorldToScreen(m_entity2WorldTranslation, m_cameraState);

        // single click select entity2
        m_actionDispatcher->SetStickySelect(false)
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // undo action
        AzToolsFramework::ToolsApplicationRequestBus::Broadcast(&AzToolsFramework::ToolsApplicationRequestBus::Events::UndoPressed);

        // entity1 is selected after undo
        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1));
    }

    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, BoundsBetweenCameraAndNearClipPlaneDoesNotIntersectMouseRay)
    {
        // move camera to 10 units along the y-axis
        AzFramework::SetCameraTransform(m_cameraState, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY(10.0f)));

        // send a very narrow bounds for entity1
        AZ::Entity* entity1 = AzToolsFramework::GetEntityById(m_entityId1);
        auto* boundTestComponent = entity1->FindComponent<BoundsTestComponent>();
        boundTestComponent->m_localBounds =
            AZ::Aabb::CreateFromMinMax(AZ::Vector3(-0.5f, -0.0025f, -0.5f), AZ::Vector3(0.5f, 0.0025f, 0.5f));

        // move entity1 in front of the camera between it and the near clip plane
        AZ::TransformBus::Event(
            m_entityId1, &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY(10.05f)));
        // move entity2 behind entity1
        AZ::TransformBus::Event(
            m_entityId2, &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY(15.0f)));

        const auto entity2ScreenPosition = AzFramework::WorldToScreen(AzToolsFramework::GetWorldTranslation(m_entityId2), m_cameraState);

        // ensure icons are not enabled to avoid them interfering with bound detection
        m_viewportManipulatorInteraction->GetViewportInteraction().SetIconsVisible(false);

        // click the entity in the viewport
        m_actionDispatcher->SetStickySelect(true)
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->CameraState(m_cameraState)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // ensure entity1 is not selected as it is before the near clip plane
        using ::testing::UnorderedElementsAreArray;
        const AzToolsFramework::EntityIdList selectedEntities = SelectedEntities();
        const AzToolsFramework::EntityIdList expectedSelectedEntities = { m_entityId2 };
        EXPECT_THAT(selectedEntities, UnorderedElementsAreArray(expectedSelectedEntities));
    }

    // entity can be selected using icon
    TEST_F(EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, CursorOverEntityIconReturnsThatEntityId)
    {
        const AZ::EntityId boundlessEntityId = CreateDefaultEditorEntity("BoundlessEntity");

        // camera (go to position format) -5.00, -8.00, 5.00, 0.00, 0.00
        AzFramework::SetCameraTransform(m_cameraState, AZ::Transform::CreateTranslation(AZ::Vector3(-5.0f, -8.0f, 5.0f)));
        // position entity in the world
        AZ::TransformBus::Event(boundlessEntityId, &AZ::TransformBus::Events::SetWorldTranslation, AZ::Vector3(-5.0f, -1.0f, 5.0f));

        const float distanceFromCamera = m_cameraState.m_position.GetDistance(AzToolsFramework::GetWorldTranslation(boundlessEntityId));

        const auto quaterIconSize = AzToolsFramework::GetIconSize(distanceFromCamera) * 0.25f;
        const auto entity1ScreenPosition =
            AzFramework::WorldToScreen(AzToolsFramework::GetWorldTranslation(boundlessEntityId), m_cameraState) +
            AzFramework::ScreenVectorFromVector2(AZ::Vector2(quaterIconSize));

        AzToolsFramework::EditorVisibleEntityDataCache editorVisibleEntityDataCache;
        AzToolsFramework::EditorHelpers editorHelpers(&editorVisibleEntityDataCache);

        const auto viewportId = m_viewportManipulatorInteraction->GetViewportInteraction().GetViewportId();
        const auto mousePick = BuildMousePick(m_cameraState, entity1ScreenPosition);
        const auto mouseInteraction = BuildMouseInteraction(
            mousePick, BuildMouseButtons(AzToolsFramework::ViewportInteraction::MouseButton::None),
            AzToolsFramework::ViewportInteraction::InteractionId(AZ::EntityId(), viewportId),
            AzToolsFramework::ViewportInteraction::KeyboardModifiers());
        const auto mouseInteractionEvent = AzToolsFramework::ViewportInteraction::BuildMouseInteractionEvent(
            mouseInteraction, AzToolsFramework::ViewportInteraction::MouseEvent::Move, false);

        // mimic mouse move
        m_actionDispatcher->CameraState(m_cameraState)->MousePosition(entity1ScreenPosition);

        // simulate hovering over an icon in the viewport
        editorVisibleEntityDataCache.CalculateVisibleEntityDatas(AzFramework::ViewportInfo{ viewportId });
        auto entityIdUnderCursor = editorHelpers.FindEntityIdUnderCursor(m_cameraState, mouseInteractionEvent);

        using ::testing::Eq;
        EXPECT_THAT(entityIdUnderCursor.EntityIdUnderCursor(), Eq(boundlessEntityId));
    }

    // overlapping icons, nearest is detected
    TEST_F(EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, CursorOverOverlappingEntityIconsReturnsClosestEntityId)
    {
        const AZ::EntityId boundlessEntityId1 = CreateDefaultEditorEntity("BoundlessEntity1");
        const AZ::EntityId boundlessEntityId2 = CreateDefaultEditorEntity("BoundlessEntity2");

        // camera (go to position format) -5.00, -8.00, 5.00, 0.00, 0.00
        AzFramework::SetCameraTransform(m_cameraState, AZ::Transform::CreateTranslation(AZ::Vector3(-5.0f, -8.0f, 5.0f)));
        // position entities in the world
        AZ::TransformBus::Event(boundlessEntityId1, &AZ::TransformBus::Events::SetWorldTranslation, AZ::Vector3(-5.0f, -1.0f, 5.0f));
        // note: boundlessEntityId2 is closer to the camera
        AZ::TransformBus::Event(boundlessEntityId2, &AZ::TransformBus::Events::SetWorldTranslation, AZ::Vector3(-5.0f, -3.0f, 5.0f));

        const float distanceFromCamera = m_cameraState.m_position.GetDistance(AzToolsFramework::GetWorldTranslation(boundlessEntityId2));

        const auto quaterIconSize = AzToolsFramework::GetIconSize(distanceFromCamera) * 0.25f;
        const auto entity2ScreenPosition =
            AzFramework::WorldToScreen(AzToolsFramework::GetWorldTranslation(boundlessEntityId2), m_cameraState) +
            AzFramework::ScreenVectorFromVector2(AZ::Vector2(quaterIconSize));

        AzToolsFramework::EditorVisibleEntityDataCache editorVisibleEntityDataCache;
        AzToolsFramework::EditorHelpers editorHelpers(&editorVisibleEntityDataCache);

        const auto viewportId = m_viewportManipulatorInteraction->GetViewportInteraction().GetViewportId();
        const auto mousePick = BuildMousePick(m_cameraState, entity2ScreenPosition);
        const auto mouseInteraction = BuildMouseInteraction(
            mousePick, BuildMouseButtons(AzToolsFramework::ViewportInteraction::MouseButton::None),
            AzToolsFramework::ViewportInteraction::InteractionId(AZ::EntityId(), viewportId),
            AzToolsFramework::ViewportInteraction::KeyboardModifiers());
        const auto mouseInteractionEvent = AzToolsFramework::ViewportInteraction::BuildMouseInteractionEvent(
            mouseInteraction, AzToolsFramework::ViewportInteraction::MouseEvent::Move, false);

        // mimic mouse move
        m_actionDispatcher->CameraState(m_cameraState)->MousePosition(entity2ScreenPosition);

        // simulate hovering over an icon in the viewport
        editorVisibleEntityDataCache.CalculateVisibleEntityDatas(AzFramework::ViewportInfo{ viewportId });
        auto entityIdUnderCursor = editorHelpers.FindEntityIdUnderCursor(m_cameraState, mouseInteractionEvent);

        using ::testing::Eq;
        EXPECT_THAT(entityIdUnderCursor.EntityIdUnderCursor(), Eq(boundlessEntityId2));
    }

    // if an entity with an icon is behind an entity with a bound, the entity with the icon will be selected
    // even if the bound is closer (this is because icons are treated as if they are on the near clip plane)
    TEST_F(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixture, FurtherAwayEntityWithIconReturnedWhenBoundEntityIsInFront)
    {
        const AZ::EntityId boundEntityId = CreateEntityWithBounds("BoundEntity");
        const AZ::EntityId boundlessEntityId = CreateDefaultEditorEntity("BoundlessEntity");

        auto* boundTestComponent = AzToolsFramework::GetEntityById(boundEntityId)->FindComponent<BoundsTestComponent>();
        boundTestComponent->m_localBounds = AZ::Aabb::CreateFromMinMax(AZ::Vector3(-1.5f, -0.5f, -0.5f), AZ::Vector3(1.5f, 0.5, 0.5f));

        // camera (go to position format) -5.00, -8.00, 5.00, 0.00, 0.00
        AzFramework::SetCameraTransform(m_cameraState, AZ::Transform::CreateTranslation(AZ::Vector3(-5.0f, -8.0f, 5.0f)));
        // position entities in the world
        AZ::TransformBus::Event(boundEntityId, &AZ::TransformBus::Events::SetWorldTranslation, AZ::Vector3(-4.0f, -3.0f, 5.0f));
        // note: boundlessEntityId2 is closer to the camera
        AZ::TransformBus::Event(boundlessEntityId, &AZ::TransformBus::Events::SetWorldTranslation, AZ::Vector3(-5.0f, -1.0f, 5.0f));

        const float distanceFromCamera = m_cameraState.m_position.GetDistance(AzToolsFramework::GetWorldTranslation(boundlessEntityId));

        const auto quaterIconSize = AzToolsFramework::GetIconSize(distanceFromCamera) * 0.25f;
        const auto entity2ScreenPosition =
            AzFramework::WorldToScreen(AzToolsFramework::GetWorldTranslation(boundlessEntityId), m_cameraState) +
            AzFramework::ScreenVectorFromVector2(AZ::Vector2(quaterIconSize));

        AzToolsFramework::EditorVisibleEntityDataCache editorVisibleEntityDataCache;
        AzToolsFramework::EditorHelpers editorHelpers(&editorVisibleEntityDataCache);

        const auto viewportId = m_viewportManipulatorInteraction->GetViewportInteraction().GetViewportId();
        const auto mousePick = BuildMousePick(m_cameraState, entity2ScreenPosition);
        const auto mouseInteraction = BuildMouseInteraction(
            mousePick, BuildMouseButtons(AzToolsFramework::ViewportInteraction::MouseButton::None),
            AzToolsFramework::ViewportInteraction::InteractionId(AZ::EntityId(), viewportId),
            AzToolsFramework::ViewportInteraction::KeyboardModifiers());
        const auto mouseInteractionEvent = AzToolsFramework::ViewportInteraction::BuildMouseInteractionEvent(
            mouseInteraction, AzToolsFramework::ViewportInteraction::MouseEvent::Move, false);

        // mimic mouse move
        m_actionDispatcher->CameraState(m_cameraState)->MousePosition(entity2ScreenPosition);

        // simulate hovering over an icon in the viewport
        editorVisibleEntityDataCache.CalculateVisibleEntityDatas(AzFramework::ViewportInfo{ viewportId });
        auto entityIdUnderCursor = editorHelpers.FindEntityIdUnderCursor(m_cameraState, mouseInteractionEvent);

        using ::testing::Eq;
        EXPECT_THAT(entityIdUnderCursor.EntityIdUnderCursor(), Eq(boundlessEntityId));
    }

    class EditorTransformComponentSelectionViewportPickingManipulatorTestFixtureParam
        : public EditorTransformComponentSelectionViewportPickingManipulatorTestFixture
        , public ::testing::WithParamInterface<bool>
    {
    };

    TEST_P(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixtureParam,
        StickyAndUnstickyDittoManipulatorToOtherEntityChangesManipulatorAndDoesNotChangeSelection)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntity(m_entityId1);

        // calculate the position in screen space of the second entity
        const auto entity2ScreenPosition = AzFramework::WorldToScreen(m_entity2WorldTranslation, m_cameraState);

        // single click select entity2
        m_actionDispatcher->SetStickySelect(GetParam())
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Control)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Alt)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        // entity1 is still selected
        using ::testing::UnorderedElementsAre;
        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1));

        AZStd::optional<AZ::Transform> manipulatorTransform;
        AzToolsFramework::EditorTransformComponentSelectionRequestBus::EventResult(
            manipulatorTransform, AzToolsFramework::GetEntityContextId(),
            &AzToolsFramework::EditorTransformComponentSelectionRequestBus::Events::GetManipulatorTransform);

        EXPECT_THAT(manipulatorTransform->GetTranslation(), IsClose(m_entity2WorldTranslation));
    }

    TEST_P(
        EditorTransformComponentSelectionViewportPickingManipulatorTestFixtureParam,
        StickyAndUnstickyDittoManipulatorToOtherEntityChangesManipulatorAndClickOffResetsManipulator)
    {
        PositionEntities();
        PositionCamera(m_cameraState);

        AzToolsFramework::SelectEntity(m_entityId1);

        // calculate the position in screen space of the second entity
        const auto entity2ScreenPosition = AzFramework::WorldToScreen(m_entity2WorldTranslation, m_cameraState);

        // position in space above the entities
        const auto clickOffPositionWorld = AZ::Vector3(5.0f, 15.0f, 12.0f);
        // calculate the screen space position of the click
        const auto clickOffPositionScreen = AzFramework::WorldToScreen(clickOffPositionWorld, m_cameraState);

        using ::testing::UnorderedElementsAre;
        // single click select entity2, then click off
        m_actionDispatcher->SetStickySelect(GetParam())
            ->CameraState(m_cameraState)
            ->MousePosition(entity2ScreenPosition)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Control)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Alt)
            ->MouseLButtonDown()
            ->MouseLButtonUp()
            ->ExecuteBlock(
                [this]()
                {
                    auto selectedEntitiesAfter = SelectedEntities();
                    EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1));

                    AZStd::optional<AZ::Transform> manipulatorTransform;
                    AzToolsFramework::EditorTransformComponentSelectionRequestBus::EventResult(
                        manipulatorTransform, AzToolsFramework::GetEntityContextId(),
                        &AzToolsFramework::EditorTransformComponentSelectionRequestBus::Events::GetManipulatorTransform);

                    EXPECT_THAT(manipulatorTransform->GetTranslation(), IsClose(m_entity2WorldTranslation));
                })
            ->MousePosition(clickOffPositionScreen)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Control)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Alt)
            ->MouseLButtonDown()
            ->MouseLButtonUp();

        auto selectedEntitiesAfter = SelectedEntities();
        EXPECT_THAT(selectedEntitiesAfter, UnorderedElementsAre(m_entityId1));

        AZStd::optional<AZ::Transform> manipulatorTransform;
        AzToolsFramework::EditorTransformComponentSelectionRequestBus::EventResult(
            manipulatorTransform, AzToolsFramework::GetEntityContextId(),
            &AzToolsFramework::EditorTransformComponentSelectionRequestBus::Events::GetManipulatorTransform);

        // manipulator transform is reset
        EXPECT_THAT(manipulatorTransform->GetTranslation(), IsClose(m_entity1WorldTranslation));
    }

    INSTANTIATE_TEST_CASE_P(All, EditorTransformComponentSelectionViewportPickingManipulatorTestFixtureParam, testing::Values(true, false));

    using EditorTransformComponentSelectionManipulatorTestFixture =
        IndirectCallManipulatorViewportInteractionFixtureMixin<EditorTransformComponentSelectionFixture>;

    TEST_F(EditorTransformComponentSelectionManipulatorTestFixture, CanMoveEntityUsingManipulatorMouseMovement)
    {
        // the initial starting position of the entity (in front and to the left of the camera)
        const auto initialTransformWorld = AZ::Transform::CreateTranslation(AZ::Vector3(-10.0f, 10.0f, 0.0f));
        // where the entity should end up (in front and to the right of the camera)
        const auto finalTransformWorld = AZ::Transform::CreateTranslation(AZ::Vector3(10.0f, 10.0f, 0.0f));

        // calculate the position in screen space of the initial position of the entity
        const auto initialPositionScreen = AzFramework::WorldToScreen(initialTransformWorld.GetTranslation(), m_cameraState);
        // calculate the position in screen space of the final position of the entity
        const auto finalPositionScreen = AzFramework::WorldToScreen(finalTransformWorld.GetTranslation(), m_cameraState);

        // select the entity (this will cause the manipulators to appear in EditorTransformComponentSelection)
        AzToolsFramework::SelectEntity(m_entityId1);
        // move the entity to its starting position
        AzToolsFramework::SetWorldTransform(m_entityId1, initialTransformWorld);

        // refresh the manipulators so that they update to the position of the entity
        // note: could skip this by selecting the entity after moving it but its useful to have this for reference
        RefreshManipulators(AzToolsFramework::EditorTransformComponentSelectionRequestBus::Events::RefreshType::All);

        // create an offset along the linear manipulator pointing along the x-axis (perpendicular to the camera view)
        const auto mouseOffsetOnManipulator = AzFramework::ScreenVector(10, 0);
        // store the mouse down position on the manipulator
        const auto mouseDownPosition = initialPositionScreen + mouseOffsetOnManipulator;
        // final position in screen space of the mouse
        const auto mouseMovePosition = finalPositionScreen + mouseOffsetOnManipulator;

        m_actionDispatcher->CameraState(m_cameraState)
            ->MousePosition(mouseDownPosition)
            ->MouseLButtonDown()
            ->MousePosition(mouseMovePosition)
            ->MouseLButtonUp();

        // read back the position of the entity now
        const AZ::Transform finalEntityTransform = AzToolsFramework::GetWorldTransform(m_entityId1);

        // ensure final world positions match
        EXPECT_THAT(finalEntityTransform, IsCloseTolerance(finalTransformWorld, 0.01f));
    }

    TEST_F(EditorTransformComponentSelectionManipulatorTestFixture, TranslatingEntityWithLinearManipulatorNotifiesOnEntityTransformChanged)
    {
        EditorEntityComponentChangeDetector editorEntityChangeDetector(m_entityId1);

        // the initial starting position of the entity (in front and to the left of the camera)
        const auto initialTransformWorld = AZ::Transform::CreateTranslation(AZ::Vector3(-10.0f, 10.0f, 0.0f));
        // where the entity should end up (in front and to the right of the camera)
        const auto finalTransformWorld = AZ::Transform::CreateTranslation(AZ::Vector3(10.0f, 10.0f, 0.0f));

        // calculate the position in screen space of the initial position of the entity
        const auto initialPositionScreen = AzFramework::WorldToScreen(initialTransformWorld.GetTranslation(), m_cameraState);
        // calculate the position in screen space of the final position of the entity
        const auto finalPositionScreen = AzFramework::WorldToScreen(finalTransformWorld.GetTranslation(), m_cameraState);

        // move the entity to its starting position
        AzToolsFramework::SetWorldTransform(m_entityId1, initialTransformWorld);
        // select the entity (this will cause the manipulators to appear in EditorTransformComponentSelection)
        AzToolsFramework::SelectEntity(m_entityId1);

        // create an offset along the linear manipulator pointing along the x-axis (perpendicular to the camera view)
        const auto mouseOffsetOnManipulator = AzFramework::ScreenVector(10, 0);
        // store the mouse down position on the manipulator
        const auto mouseDownPosition = initialPositionScreen + mouseOffsetOnManipulator;
        // final position in screen space of the mouse
        const auto mouseMovePosition = finalPositionScreen + mouseOffsetOnManipulator;

        m_actionDispatcher->CameraState(m_cameraState)
            ->MousePosition(mouseDownPosition)
            ->MouseLButtonDown()
            ->MousePosition(mouseMovePosition)
            ->MouseLButtonUp();

        // verify a EditorTransformChangeNotificationBus::OnEntityTransformChanged occurred
        using ::testing::UnorderedElementsAreArray;
        EXPECT_THAT(editorEntityChangeDetector.m_entityIds, UnorderedElementsAreArray(m_entityIds));
    }

    // simple widget to listen for a mouse wheel event and then forward it on to the ViewportSelectionRequestBus
    class WheelEventWidget : public QWidget
    {
        using MouseInteractionResult = AzToolsFramework::ViewportInteraction::MouseInteractionResult;

    public:
        WheelEventWidget(const AzFramework::ViewportId viewportId, QWidget* parent = nullptr)
            : QWidget(parent)
            , m_viewportId(viewportId)
        {
        }

        void wheelEvent(QWheelEvent* ev) override
        {
            namespace vi = AzToolsFramework::ViewportInteraction;
            vi::MouseInteraction mouseInteraction;
            mouseInteraction.m_interactionId.m_cameraId = AZ::EntityId();
            mouseInteraction.m_interactionId.m_viewportId = m_viewportId;
            mouseInteraction.m_mouseButtons = vi::BuildMouseButtons(ev->buttons());
            mouseInteraction.m_mousePick = vi::MousePick();
            mouseInteraction.m_keyboardModifiers = vi::BuildKeyboardModifiers(ev->modifiers());

            AzToolsFramework::EditorInteractionSystemViewportSelectionRequestBus::EventResult(
                m_mouseInteractionResult, AzToolsFramework::GetEntityContextId(),
                &AzToolsFramework::EditorInteractionSystemViewportSelectionRequestBus::Events::InternalHandleAllMouseInteractions,
                vi::MouseInteractionEvent(mouseInteraction, static_cast<float>(ev->angleDelta().y())));
        }

        MouseInteractionResult m_mouseInteractionResult;
        AzFramework::ViewportId m_viewportId;
    };

    TEST_F(EditorTransformComponentSelectionManipulatorTestFixture, MouseScrollWheelSwitchesTransformMode)
    {
        namespace vi = AzToolsFramework::ViewportInteraction;
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        const auto transformMode = []
        {
            EditorTransformComponentSelectionRequestBus::Events::Mode transformMode;
            EditorTransformComponentSelectionRequestBus::EventResult(
                transformMode, AzToolsFramework::GetEntityContextId(),
                &EditorTransformComponentSelectionRequestBus::Events::GetTransformMode);
            return transformMode;
        };

        // given
        // preconditions
        EXPECT_THAT(transformMode(), EditorTransformComponentSelectionRequestBus::Events::Mode::Translation);

        auto wheelEventWidget = WheelEventWidget(m_viewportManipulatorInteraction->GetViewportInteraction().GetViewportId());
        // attach the global event filter to the placeholder widget
        AzQtComponents::GlobalEventFilter globalEventFilter(QApplication::instance());
        wheelEventWidget.installEventFilter(&globalEventFilter);

        // example mouse wheel event (does not yet factor in position of mouse in relation to widget)
        auto wheelEvent = QWheelEvent(
            QPointF(0.0f, 0.0f), QPointF(0.0f, 0.0f), QPoint(0, 1), QPoint(0, 0), Qt::MouseButton::NoButton,
            Qt::KeyboardModifier::ControlModifier, Qt::ScrollPhase::ScrollBegin, false,
            Qt::MouseEventSource::MouseEventSynthesizedBySystem);

        // when (trigger mouse wheel event)
        QApplication::sendEvent(&wheelEventWidget, &wheelEvent);

        // then
        // transform mode has changed and mouse event was handled
        using ::testing::Eq;
        EXPECT_THAT(transformMode(), Eq(EditorTransformComponentSelectionRequestBus::Events::Mode::Rotation));
        EXPECT_THAT(wheelEventWidget.m_mouseInteractionResult, Eq(vi::MouseInteractionResult::Viewport));
    }

    TEST_F(EditorTransformComponentSelectionFixture, EntityPositionsCanBeSnappedToGrid)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;
        using ::testing::Pointwise;

        m_entityIds.push_back(CreateDefaultEditorEntity("Entity2"));
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity3"));

        const AZStd::vector<AZ::Vector3> initialUnsnappedPositions = { AZ::Vector3(1.2f, 3.5f, 6.7f), AZ::Vector3(13.2f, 15.6f, 11.4f),
                                                                       AZ::Vector3(4.2f, 103.2f, 16.6f) };
        AZ::TransformBus::Event(m_entityIds[0], &AZ::TransformBus::Events::SetWorldTranslation, initialUnsnappedPositions[0]);
        AZ::TransformBus::Event(m_entityIds[1], &AZ::TransformBus::Events::SetWorldTranslation, initialUnsnappedPositions[1]);
        AZ::TransformBus::Event(m_entityIds[2], &AZ::TransformBus::Events::SetWorldTranslation, initialUnsnappedPositions[2]);

        AzToolsFramework::SelectEntities(m_entityIds);

        EditorTransformComponentSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(), &EditorTransformComponentSelectionRequestBus::Events::SnapSelectedEntitiesToWorldGrid,
            2.0f);

        AZStd::vector<AZ::Vector3> entityPositionsAfterSnap;
        AZStd::transform(
            m_entityIds.cbegin(), m_entityIds.cend(), AZStd::back_inserter(entityPositionsAfterSnap),
            [](const AZ::EntityId& entityId)
            {
                return AzToolsFramework::GetWorldTranslation(entityId);
            });

        const AZStd::vector<AZ::Vector3> expectedSnappedPositions = { AZ::Vector3(2.0f, 4.0f, 6.0f), AZ::Vector3(14.0f, 16.0f, 12.0f),
                                                                      AZ::Vector3(4.0f, 104.0f, 16.0f) };
        EXPECT_THAT(entityPositionsAfterSnap, Pointwise(ContainerIsClose(), expectedSnappedPositions));
    }

    TEST_F(EditorTransformComponentSelectionFixture, ManipulatorStaysAlignedToEntityTranslationAfterSnap)
    {
        using AzToolsFramework::EditorTransformComponentSelectionRequestBus;

        const auto initialUnsnappedPosition = AZ::Vector3(1.2f, 3.5f, 6.7f);
        AZ::TransformBus::Event(m_entityIds[0], &AZ::TransformBus::Events::SetWorldTranslation, initialUnsnappedPosition);

        AzToolsFramework::SelectEntities(m_entityIds);

        EditorTransformComponentSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(), &EditorTransformComponentSelectionRequestBus::Events::SnapSelectedEntitiesToWorldGrid,
            1.0f);

        const auto entityPositionAfterSnap = AzToolsFramework::GetWorldTranslation(m_entityId1);
        const AZ::Vector3 manipulatorPositionAfterSnap =
            GetManipulatorTransform().value_or(AZ::Transform::CreateIdentity()).GetTranslation();

        const auto expectedSnappedPosition = AZ::Vector3(1.0f, 4.0f, 7.0f);
        EXPECT_THAT(entityPositionAfterSnap, IsClose(expectedSnappedPosition));
        EXPECT_THAT(expectedSnappedPosition, IsClose(manipulatorPositionAfterSnap));
    }

    // struct to contain input reference frame and expected orientation outcome based on
    // the reference frame, selection and entity hierarchy
    struct ReferenceFrameWithOrientation
    {
        AzToolsFramework::ReferenceFrame m_referenceFrame; // the input reference frame (Local/Parent/World)
        AZ::Quaternion m_orientation; // the orientation of the manipulator transform
    };

    // custom orientation to compare against for leaf/child entities (when ReferenceFrame is Local)
    static const AZ::Quaternion ChildExpectedPivotLocalOrientationInWorldSpace =
        AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisZ(), AZ::DegToRad(45.0f));

    // custom orientation to compare against for branch/parent entities (when ReferenceFrame is Parent)
    static const AZ::Quaternion ParentExpectedPivotLocalOrientationInWorldSpace =
        AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisX(), AZ::DegToRad(45.0f));

    // custom orientation to compare against for orientation/pivot override
    static const AZ::Quaternion PivotOverrideLocalOrientationInWorldSpace =
        AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisY(), AZ::DegToRad(90.0f));

    class EditorTransformComponentSelectionSingleEntityPivotFixture
        : public EditorTransformComponentSelectionFixture
        , public ::testing::WithParamInterface<ReferenceFrameWithOrientation>
    {
    };

    TEST_P(EditorTransformComponentSelectionSingleEntityPivotFixture, PivotOrientationMatchesReferenceFrameSingleEntity)
    {
        using AzToolsFramework::ETCS::CalculatePivotOrientation;
        using AzToolsFramework::ETCS::PivotOrientationResult;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        AZ::TransformBus::Event(
            m_entityIds[0], &AZ::TransformBus::Events::SetWorldTM,
            AZ::Transform::CreateFromQuaternionAndTranslation(ChildExpectedPivotLocalOrientationInWorldSpace, AZ::Vector3::CreateZero()));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        const ReferenceFrameWithOrientation referenceFrameWithOrientation = GetParam();

        const PivotOrientationResult pivotResult =
            CalculatePivotOrientation(m_entityIds[0], referenceFrameWithOrientation.m_referenceFrame);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_THAT(pivotResult.m_worldOrientation, IsClose(referenceFrameWithOrientation.m_orientation));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    INSTANTIATE_TEST_CASE_P(
        All,
        EditorTransformComponentSelectionSingleEntityPivotFixture,
        testing::Values(
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Local, ChildExpectedPivotLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Parent, AZ::Quaternion::CreateIdentity() },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::World, AZ::Quaternion::CreateIdentity() }));

    class EditorTransformComponentSelectionSingleEntityWithParentPivotFixture
        : public EditorTransformComponentSelectionFixture
        , public ::testing::WithParamInterface<ReferenceFrameWithOrientation>
    {
    };

    TEST_P(EditorTransformComponentSelectionSingleEntityWithParentPivotFixture, PivotOrientationMatchesReferenceFrameEntityWithParent)
    {
        using AzToolsFramework::ETCS::CalculatePivotOrientation;
        using AzToolsFramework::ETCS::PivotOrientationResult;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        const AZ::EntityId parentEntityId = CreateDefaultEditorEntity("Parent");
        AZ::TransformBus::Event(m_entityIds[0], &AZ::TransformBus::Events::SetParent, parentEntityId);

        AZ::TransformBus::Event(
            parentEntityId, &AZ::TransformBus::Events::SetWorldTM,
            AZ::Transform::CreateFromQuaternionAndTranslation(ParentExpectedPivotLocalOrientationInWorldSpace, AZ::Vector3::CreateZero()));

        AZ::TransformBus::Event(
            m_entityIds[0], &AZ::TransformBus::Events::SetWorldTM,
            AZ::Transform::CreateFromQuaternionAndTranslation(
                ChildExpectedPivotLocalOrientationInWorldSpace, AZ::Vector3::CreateAxisZ(-5.0f)));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        const ReferenceFrameWithOrientation referenceFrameWithOrientation = GetParam();

        const PivotOrientationResult pivotResult =
            CalculatePivotOrientation(m_entityIds[0], referenceFrameWithOrientation.m_referenceFrame);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_THAT(pivotResult.m_worldOrientation, IsClose(referenceFrameWithOrientation.m_orientation));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // with a single entity selected with a parent the orientation reference frames follow as you'd expect
    INSTANTIATE_TEST_CASE_P(
        All,
        EditorTransformComponentSelectionSingleEntityWithParentPivotFixture,
        testing::Values(
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Local, ChildExpectedPivotLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Parent, ParentExpectedPivotLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::World, AZ::Quaternion::CreateIdentity() }));

    class EditorTransformComponentSelectionMultipleEntitiesPivotFixture
        : public EditorTransformComponentSelectionFixture
        , public ::testing::WithParamInterface<ReferenceFrameWithOrientation>
    {
    };

    TEST_P(EditorTransformComponentSelectionMultipleEntitiesPivotFixture, PivotOrientationMatchesReferenceFrameMultipleEntities)
    {
        using AzToolsFramework::ETCS::CalculatePivotOrientationForEntityIds;
        using AzToolsFramework::ETCS::PivotOrientationResult;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity2"));
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity3"));

        // setup entities in arbitrary triangle arrangement
        AZ::TransformBus::Event(
            m_entityIds[0], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(-10.0f)));

        AZ::TransformBus::Event(
            m_entityIds[1], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(10.0f)));

        AZ::TransformBus::Event(
            m_entityIds[2], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY(10.0f)));

        using AzToolsFramework::EntityIdManipulatorLookup;
        // note: EntityIdManipulatorLookup{} is unused during this test
        AzToolsFramework::EntityIdManipulatorLookups lookups{ { m_entityIds[0], EntityIdManipulatorLookup{} },
                                                              { m_entityIds[1], EntityIdManipulatorLookup{} },
                                                              { m_entityIds[2], EntityIdManipulatorLookup{} } };
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        const ReferenceFrameWithOrientation referenceFrameWithOrientation = GetParam();

        const PivotOrientationResult pivotResult =
            CalculatePivotOrientationForEntityIds(lookups, referenceFrameWithOrientation.m_referenceFrame);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_THAT(pivotResult.m_worldOrientation, IsClose(referenceFrameWithOrientation.m_orientation));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // with a group selection, when the entities are not in a hierarchy, no matter what reference frame,
    // we will always get an orientation aligned to the world
    INSTANTIATE_TEST_CASE_P(
        All,
        EditorTransformComponentSelectionMultipleEntitiesPivotFixture,
        testing::Values(
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Local, AZ::Quaternion::CreateIdentity() },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Parent, AZ::Quaternion::CreateIdentity() },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::World, AZ::Quaternion::CreateIdentity() }));

    class EditorTransformComponentSelectionMultipleEntitiesWithSameParentPivotFixture
        : public EditorTransformComponentSelectionFixture
        , public ::testing::WithParamInterface<ReferenceFrameWithOrientation>
    {
    };

    TEST_P(
        EditorTransformComponentSelectionMultipleEntitiesWithSameParentPivotFixture,
        PivotOrientationMatchesReferenceFrameMultipleEntitiesSameParent)
    {
        using AzToolsFramework::ETCS::CalculatePivotOrientationForEntityIds;
        using AzToolsFramework::ETCS::PivotOrientationResult;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity2"));
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity3"));

        AZ::TransformBus::Event(
            m_entityIds[0], &AZ::TransformBus::Events::SetWorldTM,
            AZ::Transform::CreateFromQuaternionAndTranslation(
                ParentExpectedPivotLocalOrientationInWorldSpace, AZ::Vector3::CreateAxisZ(-5.0f)));

        AZ::TransformBus::Event(
            m_entityIds[1], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(10.0f)));

        AZ::TransformBus::Event(
            m_entityIds[2], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY(10.0f)));

        AZ::TransformBus::Event(m_entityIds[1], &AZ::TransformBus::Events::SetParent, m_entityIds[0]);
        AZ::TransformBus::Event(m_entityIds[2], &AZ::TransformBus::Events::SetParent, m_entityIds[0]);

        using AzToolsFramework::EntityIdManipulatorLookup;
        // note: EntityIdManipulatorLookup{} is unused during this test
        // only select second two entities that are children of m_entityIds[0]
        AzToolsFramework::EntityIdManipulatorLookups lookups{ { m_entityIds[1], EntityIdManipulatorLookup{} },
                                                              { m_entityIds[2], EntityIdManipulatorLookup{} } };
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        const ReferenceFrameWithOrientation referenceFrameWithOrientation = GetParam();

        const PivotOrientationResult pivotResult =
            CalculatePivotOrientationForEntityIds(lookups, referenceFrameWithOrientation.m_referenceFrame);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_THAT(pivotResult.m_worldOrientation, IsClose(referenceFrameWithOrientation.m_orientation));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // here two entities are selected with the same parent - local and parent will match parent space, with world
    // giving the identity (aligned to world axes)
    INSTANTIATE_TEST_CASE_P(
        All,
        EditorTransformComponentSelectionMultipleEntitiesWithSameParentPivotFixture,
        testing::Values(
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Local, ParentExpectedPivotLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Parent, ParentExpectedPivotLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::World, AZ::Quaternion::CreateIdentity() }));

    class EditorTransformComponentSelectionMultipleEntitiesWithDifferentParentPivotFixture
        : public EditorTransformComponentSelectionFixture
        , public ::testing::WithParamInterface<ReferenceFrameWithOrientation>
    {
    };

    TEST_P(
        EditorTransformComponentSelectionMultipleEntitiesWithDifferentParentPivotFixture,
        PivotOrientationMatchesReferenceFrameMultipleEntitiesDifferentParent)
    {
        using AzToolsFramework::ETCS::CalculatePivotOrientationForEntityIds;
        using AzToolsFramework::ETCS::PivotOrientationResult;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity2"));
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity3"));
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity4"));

        AZ::TransformBus::Event(
            m_entityIds[0], &AZ::TransformBus::Events::SetWorldTM,
            AZ::Transform::CreateFromQuaternionAndTranslation(
                ParentExpectedPivotLocalOrientationInWorldSpace, AZ::Vector3::CreateAxisZ(-5.0f)));

        AZ::TransformBus::Event(
            m_entityIds[1], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(10.0f)));

        AZ::TransformBus::Event(
            m_entityIds[2], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY(10.0f)));

        AZ::TransformBus::Event(m_entityIds[1], &AZ::TransformBus::Events::SetParent, m_entityIds[0]);
        AZ::TransformBus::Event(m_entityIds[2], &AZ::TransformBus::Events::SetParent, m_entityIds[3]);

        using AzToolsFramework::EntityIdManipulatorLookup;
        // note: EntityIdManipulatorLookup{} is unused during this test
        // only select second two entities that are children of different m_entities
        AzToolsFramework::EntityIdManipulatorLookups lookups{ { m_entityIds[1], EntityIdManipulatorLookup{} },
                                                              { m_entityIds[2], EntityIdManipulatorLookup{} } };
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        const ReferenceFrameWithOrientation referenceFrameWithOrientation = GetParam();

        const PivotOrientationResult pivotResult =
            CalculatePivotOrientationForEntityIds(lookups, referenceFrameWithOrientation.m_referenceFrame);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_THAT(pivotResult.m_worldOrientation, IsClose(referenceFrameWithOrientation.m_orientation));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // if multiple entities are selected without a parent in common, orientation will always be world again
    INSTANTIATE_TEST_CASE_P(
        All,
        EditorTransformComponentSelectionMultipleEntitiesWithDifferentParentPivotFixture,
        testing::Values(
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Local, AZ::Quaternion::CreateIdentity() },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Parent, AZ::Quaternion::CreateIdentity() },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::World, AZ::Quaternion::CreateIdentity() }));

    class EditorTransformComponentSelectionSingleEntityPivotAndOverrideFixture
        : public EditorTransformComponentSelectionFixture
        , public ::testing::WithParamInterface<ReferenceFrameWithOrientation>
    {
    };

    TEST_P(
        EditorTransformComponentSelectionSingleEntityPivotAndOverrideFixture,
        PivotOrientationMatchesReferenceFrameSingleEntityOptionalOverride)
    {
        using AzToolsFramework::ETCS::CalculateSelectionPivotOrientation;
        using AzToolsFramework::ETCS::PivotOrientationResult;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        AZ::TransformBus::Event(
            m_entityIds[0], &AZ::TransformBus::Events::SetWorldTM,
            AZ::Transform::CreateFromQuaternionAndTranslation(ChildExpectedPivotLocalOrientationInWorldSpace, AZ::Vector3::CreateZero()));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        const ReferenceFrameWithOrientation referenceFrameWithOrientation = GetParam();

        AzToolsFramework::EntityIdManipulatorLookups lookups{ { m_entityIds[0], AzToolsFramework::EntityIdManipulatorLookup{} } };

        // set override frame (orientation only)
        AzToolsFramework::OptionalFrame optionalFrame;
        optionalFrame.m_orientationOverride = PivotOverrideLocalOrientationInWorldSpace;

        const PivotOrientationResult pivotResult =
            CalculateSelectionPivotOrientation(lookups, optionalFrame, referenceFrameWithOrientation.m_referenceFrame);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_THAT(pivotResult.m_worldOrientation, IsClose(referenceFrameWithOrientation.m_orientation));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // local reference frame will still return local orientation for entity, but pivot override will trump parent
    // space (world will still give identity alignment for axes)
    INSTANTIATE_TEST_CASE_P(
        All,
        EditorTransformComponentSelectionSingleEntityPivotAndOverrideFixture,
        testing::Values(
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Local, ChildExpectedPivotLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Parent, PivotOverrideLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::World, AZ::Quaternion::CreateIdentity() }));

    class EditorTransformComponentSelectionMultipleEntitiesPivotAndOverrideFixture
        : public EditorTransformComponentSelectionFixture
        , public ::testing::WithParamInterface<ReferenceFrameWithOrientation>
    {
    };

    TEST_P(
        EditorTransformComponentSelectionMultipleEntitiesPivotAndOverrideFixture,
        PivotOrientationMatchesReferenceFrameMultipleEntitiesOptionalOverride)
    {
        using AzToolsFramework::ETCS::CalculateSelectionPivotOrientation;
        using AzToolsFramework::ETCS::PivotOrientationResult;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity2"));
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity3"));

        AZ::TransformBus::Event(
            m_entityIds[0], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(-10.0f)));

        AZ::TransformBus::Event(
            m_entityIds[1], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(10.0f)));

        AZ::TransformBus::Event(
            m_entityIds[2], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY(10.0f)));

        using AzToolsFramework::EntityIdManipulatorLookup;
        // note: EntityIdManipulatorLookup{} is unused during this test
        AzToolsFramework::EntityIdManipulatorLookups lookups{ { m_entityIds[0], EntityIdManipulatorLookup{} },
                                                              { m_entityIds[1], EntityIdManipulatorLookup{} },
                                                              { m_entityIds[2], EntityIdManipulatorLookup{} } };
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        const ReferenceFrameWithOrientation referenceFrameWithOrientation = GetParam();

        AzToolsFramework::OptionalFrame optionalFrame;
        optionalFrame.m_orientationOverride = PivotOverrideLocalOrientationInWorldSpace;

        const PivotOrientationResult pivotResult =
            CalculateSelectionPivotOrientation(lookups, optionalFrame, referenceFrameWithOrientation.m_referenceFrame);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_THAT(pivotResult.m_worldOrientation, IsClose(referenceFrameWithOrientation.m_orientation));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // with multiple entities selected, override frame wins in both local and parent reference frames
    INSTANTIATE_TEST_CASE_P(
        All,
        EditorTransformComponentSelectionMultipleEntitiesPivotAndOverrideFixture,
        testing::Values(
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Local, PivotOverrideLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Parent, PivotOverrideLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::World, AZ::Quaternion::CreateIdentity() }));

    class EditorTransformComponentSelectionMultipleEntitiesPivotAndNoOverrideFixture
        : public EditorTransformComponentSelectionFixture
        , public ::testing::WithParamInterface<ReferenceFrameWithOrientation>
    {
    };

    TEST_P(
        EditorTransformComponentSelectionMultipleEntitiesPivotAndNoOverrideFixture,
        PivotOrientationMatchesReferenceFrameMultipleEntitiesNoOptionalOverride)
    {
        using AzToolsFramework::ETCS::CalculateSelectionPivotOrientation;
        using AzToolsFramework::ETCS::PivotOrientationResult;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity2"));
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity3"));

        AZ::TransformBus::Event(
            m_entityIds[0], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(-10.0f)));

        AZ::TransformBus::Event(
            m_entityIds[1], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(10.0f)));

        AZ::TransformBus::Event(
            m_entityIds[2], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY(10.0f)));

        using AzToolsFramework::EntityIdManipulatorLookup;
        // note: EntityIdManipulatorLookup{} is unused during this test
        AzToolsFramework::EntityIdManipulatorLookups lookups{ { m_entityIds[0], EntityIdManipulatorLookup{} },
                                                              { m_entityIds[1], EntityIdManipulatorLookup{} },
                                                              { m_entityIds[2], EntityIdManipulatorLookup{} } };
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        const ReferenceFrameWithOrientation referenceFrameWithOrientation = GetParam();

        AzToolsFramework::OptionalFrame optionalFrame;
        const PivotOrientationResult pivotResult =
            CalculateSelectionPivotOrientation(lookups, optionalFrame, referenceFrameWithOrientation.m_referenceFrame);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_THAT(pivotResult.m_worldOrientation, IsClose(referenceFrameWithOrientation.m_orientation));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // multiple entities selected (no hierarchy) always get world aligned axes (identity)
    INSTANTIATE_TEST_CASE_P(
        All,
        EditorTransformComponentSelectionMultipleEntitiesPivotAndNoOverrideFixture,
        testing::Values(
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Local, AZ::Quaternion::CreateIdentity() },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Parent, AZ::Quaternion::CreateIdentity() },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::World, AZ::Quaternion::CreateIdentity() }));

    class EditorTransformComponentSelectionMultipleEntitiesSameParentPivotAndNoOverrideFixture
        : public EditorTransformComponentSelectionFixture
        , public ::testing::WithParamInterface<ReferenceFrameWithOrientation>
    {
    };

    TEST_P(
        EditorTransformComponentSelectionMultipleEntitiesSameParentPivotAndNoOverrideFixture,
        PivotOrientationMatchesReferenceFrameMultipleEntitiesSameParentNoOptionalOverride)
    {
        using AzToolsFramework::ETCS::CalculateSelectionPivotOrientation;
        using AzToolsFramework::ETCS::PivotOrientationResult;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity2"));
        m_entityIds.push_back(CreateDefaultEditorEntity("Entity3"));

        AZ::TransformBus::Event(
            m_entityIds[0], &AZ::TransformBus::Events::SetWorldTM,
            AZ::Transform::CreateFromQuaternionAndTranslation(
                ParentExpectedPivotLocalOrientationInWorldSpace, AZ::Vector3::CreateAxisZ(-5.0f)));

        AZ::TransformBus::Event(
            m_entityIds[1], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(10.0f)));

        AZ::TransformBus::Event(
            m_entityIds[2], &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY(10.0f)));

        AZ::TransformBus::Event(m_entityIds[1], &AZ::TransformBus::Events::SetParent, m_entityIds[0]);
        AZ::TransformBus::Event(m_entityIds[2], &AZ::TransformBus::Events::SetParent, m_entityIds[0]);

        using AzToolsFramework::EntityIdManipulatorLookup;
        // note: EntityIdManipulatorLookup{} is unused during this test
        AzToolsFramework::EntityIdManipulatorLookups lookups{ { m_entityIds[1], EntityIdManipulatorLookup{} },
                                                              { m_entityIds[2], EntityIdManipulatorLookup{} } };
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        const ReferenceFrameWithOrientation referenceFrameWithOrientation = GetParam();

        AzToolsFramework::OptionalFrame optionalFrame;
        const PivotOrientationResult pivotResult =
            CalculateSelectionPivotOrientation(lookups, optionalFrame, referenceFrameWithOrientation.m_referenceFrame);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_THAT(pivotResult.m_worldOrientation, IsClose(referenceFrameWithOrientation.m_orientation));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // no optional frame, same parent, local and parent both get parent alignment (world reference frame
    // gives world alignment (identity))
    INSTANTIATE_TEST_CASE_P(
        All,
        EditorTransformComponentSelectionMultipleEntitiesSameParentPivotAndNoOverrideFixture,
        testing::Values(
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Local, ParentExpectedPivotLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::Parent, ParentExpectedPivotLocalOrientationInWorldSpace },
            ReferenceFrameWithOrientation{ AzToolsFramework::ReferenceFrame::World, AZ::Quaternion::CreateIdentity() }));

    class EditorEntityModelVisibilityFixture
        : public ToolsApplicationFixture
        , private AzToolsFramework::EditorEntityVisibilityNotificationBus::Router
        , private AzToolsFramework::EditorEntityInfoNotificationBus::Handler
    {
    public:
        void SetUpEditorFixtureImpl() override
        {
            AzToolsFramework::EditorEntityVisibilityNotificationBus::Router::BusRouterConnect();
            AzToolsFramework::EditorEntityInfoNotificationBus::Handler::BusConnect();
        }

        void TearDownEditorFixtureImpl() override
        {
            AzToolsFramework::EditorEntityInfoNotificationBus::Handler::BusDisconnect();
            AzToolsFramework::EditorEntityVisibilityNotificationBus::Router::BusRouterDisconnect();
        }

        bool m_entityInfoUpdatedVisibilityForLayer = false;
        AZ::EntityId m_layerId;

    private:
        // EditorEntityVisibilityNotificationBus overrides ...
        void OnEntityVisibilityChanged([[maybe_unused]] bool visibility) override
        {
            // for debug purposes
        }

        // EditorEntityInfoNotificationBus overrides ...
        void OnEntityInfoUpdatedVisibility(AZ::EntityId entityId, [[maybe_unused]] bool visible) override
        {
            if (entityId == m_layerId)
            {
                m_entityInfoUpdatedVisibilityForLayer = true;
            }
        }
    };

    // all entities in a layer are the same state, modifying the layer
    // will also notify the UI to refresh
    TEST_F(EditorEntityModelVisibilityFixture, LayerVisibilityNotifiesEditorEntityModelState)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        const AZ::EntityId a = CreateDefaultEditorEntity("A");
        const AZ::EntityId b = CreateDefaultEditorEntity("B");
        const AZ::EntityId c = CreateDefaultEditorEntity("C");

        m_layerId = CreateEditorLayerEntity("Layer");

        AZ::TransformBus::Event(a, &AZ::TransformBus::Events::SetParent, m_layerId);
        AZ::TransformBus::Event(b, &AZ::TransformBus::Events::SetParent, m_layerId);
        AZ::TransformBus::Event(c, &AZ::TransformBus::Events::SetParent, m_layerId);
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        AzToolsFramework::SetEntityVisibility(a, false);
        AzToolsFramework::SetEntityVisibility(b, false);
        AzToolsFramework::SetEntityVisibility(c, false);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_FALSE(AzToolsFramework::IsEntityVisible(a));
        EXPECT_FALSE(AzToolsFramework::IsEntityVisible(b));
        EXPECT_FALSE(AzToolsFramework::IsEntityVisible(c));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        AzToolsFramework::SetEntityVisibility(m_layerId, false);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_FALSE(AzToolsFramework::IsEntityVisible(m_layerId));
        EXPECT_TRUE(m_entityInfoUpdatedVisibilityForLayer);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // reset property
        m_entityInfoUpdatedVisibilityForLayer = false;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        AzToolsFramework::SetEntityVisibility(m_layerId, true);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(m_entityInfoUpdatedVisibilityForLayer);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(EditorEntityModelVisibilityFixture, UnhidingEntityInInvisibleLayerUnhidesAllEntitiesThatWereNotIndividuallyHidden)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        const AZ::EntityId a = CreateDefaultEditorEntity("A");
        const AZ::EntityId b = CreateDefaultEditorEntity("B");
        const AZ::EntityId c = CreateDefaultEditorEntity("C");

        const AZ::EntityId d = CreateDefaultEditorEntity("D");
        const AZ::EntityId e = CreateDefaultEditorEntity("E");
        const AZ::EntityId f = CreateDefaultEditorEntity("F");

        m_layerId = CreateEditorLayerEntity("Layer1");
        const AZ::EntityId secondLayerId = CreateEditorLayerEntity("Layer2");

        AZ::TransformBus::Event(a, &AZ::TransformBus::Events::SetParent, m_layerId);
        AZ::TransformBus::Event(b, &AZ::TransformBus::Events::SetParent, m_layerId);
        AZ::TransformBus::Event(c, &AZ::TransformBus::Events::SetParent, m_layerId);

        AZ::TransformBus::Event(secondLayerId, &AZ::TransformBus::Events::SetParent, m_layerId);

        AZ::TransformBus::Event(d, &AZ::TransformBus::Events::SetParent, secondLayerId);
        AZ::TransformBus::Event(e, &AZ::TransformBus::Events::SetParent, secondLayerId);
        AZ::TransformBus::Event(f, &AZ::TransformBus::Events::SetParent, secondLayerId);

        // Layer1
        // A
        // B
        // C
        // Layer2
        // D
        // E
        // F

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // hide top layer
        AzToolsFramework::SetEntityVisibility(m_layerId, false);

        // hide a and c (a and see are 'set' not to be visible and are not visible)
        AzToolsFramework::SetEntityVisibility(a, false);
        AzToolsFramework::SetEntityVisibility(c, false);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(!AzToolsFramework::IsEntityVisible(a));
        EXPECT_TRUE(!AzToolsFramework::IsEntitySetToBeVisible(a));

        // b will not be visible but is not 'set' to be hidden
        EXPECT_TRUE(!AzToolsFramework::IsEntityVisible(b));
        EXPECT_TRUE(AzToolsFramework::IsEntitySetToBeVisible(b));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // same for nested layer
        AzToolsFramework::SetEntityVisibility(secondLayerId, false);

        AzToolsFramework::SetEntityVisibility(d, false);
        AzToolsFramework::SetEntityVisibility(f, false);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(!AzToolsFramework::IsEntityVisible(e));
        EXPECT_TRUE(AzToolsFramework::IsEntitySetToBeVisible(e));

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // set visibility of most nested entity to true
        AzToolsFramework::SetEntityVisibility(d, true);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(AzToolsFramework::IsEntitySetToBeVisible(m_layerId));
        EXPECT_TRUE(AzToolsFramework::IsEntitySetToBeVisible(secondLayerId));

        // a will still be set to be not visible and won't be visible as parent layer is now visible
        EXPECT_TRUE(!AzToolsFramework::IsEntitySetToBeVisible(a));
        EXPECT_TRUE(!AzToolsFramework::IsEntityVisible(a));

        // b will now be visible as it was not individually
        // set to be visible and the parent layer is now visible
        EXPECT_TRUE(AzToolsFramework::IsEntitySetToBeVisible(b));
        EXPECT_TRUE(AzToolsFramework::IsEntityVisible(b));

        // same story for e as for b
        EXPECT_TRUE(AzToolsFramework::IsEntitySetToBeVisible(e));
        EXPECT_TRUE(AzToolsFramework::IsEntityVisible(e));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(EditorEntityModelVisibilityFixture, UnlockingEntityInLockedLayerUnlocksAllEntitiesThatWereNotIndividuallyLocked)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        const AZ::EntityId a = CreateDefaultEditorEntity("A");
        const AZ::EntityId b = CreateDefaultEditorEntity("B");
        const AZ::EntityId c = CreateDefaultEditorEntity("C");

        const AZ::EntityId d = CreateDefaultEditorEntity("D");
        const AZ::EntityId e = CreateDefaultEditorEntity("E");
        const AZ::EntityId f = CreateDefaultEditorEntity("F");

        m_layerId = CreateEditorLayerEntity("Layer1");
        const AZ::EntityId secondLayerId = CreateEditorLayerEntity("Layer2");

        AZ::TransformBus::Event(a, &AZ::TransformBus::Events::SetParent, m_layerId);
        AZ::TransformBus::Event(b, &AZ::TransformBus::Events::SetParent, m_layerId);
        AZ::TransformBus::Event(c, &AZ::TransformBus::Events::SetParent, m_layerId);

        AZ::TransformBus::Event(secondLayerId, &AZ::TransformBus::Events::SetParent, m_layerId);

        AZ::TransformBus::Event(d, &AZ::TransformBus::Events::SetParent, secondLayerId);
        AZ::TransformBus::Event(e, &AZ::TransformBus::Events::SetParent, secondLayerId);
        AZ::TransformBus::Event(f, &AZ::TransformBus::Events::SetParent, secondLayerId);

        // Layer1
        // A
        // B
        // C
        // Layer2
        // D
        // E
        // F

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // lock top layer
        AzToolsFramework::SetEntityLockState(m_layerId, true);

        // lock a and c (a and see are 'set' not to be visible and are not visible)
        AzToolsFramework::SetEntityLockState(a, true);
        AzToolsFramework::SetEntityLockState(c, true);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(AzToolsFramework::IsEntityLocked(a));
        EXPECT_TRUE(AzToolsFramework::IsEntitySetToBeLocked(a));

        // b will be locked but is not 'set' to be locked
        EXPECT_TRUE(AzToolsFramework::IsEntityLocked(b));
        EXPECT_TRUE(!AzToolsFramework::IsEntitySetToBeLocked(b));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // same for nested layer
        AzToolsFramework::SetEntityLockState(secondLayerId, true);

        AzToolsFramework::SetEntityLockState(d, true);
        AzToolsFramework::SetEntityLockState(f, true);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(AzToolsFramework::IsEntityLocked(e));
        EXPECT_TRUE(!AzToolsFramework::IsEntitySetToBeLocked(e));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // set visibility of most nested entity to true
        AzToolsFramework::SetEntityLockState(d, false);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(!AzToolsFramework::IsEntitySetToBeLocked(m_layerId));
        EXPECT_TRUE(!AzToolsFramework::IsEntitySetToBeLocked(secondLayerId));

        // a will still be set to be not visible and won't be visible as parent layer is now visible
        EXPECT_TRUE(AzToolsFramework::IsEntitySetToBeLocked(a));
        EXPECT_TRUE(AzToolsFramework::IsEntityLocked(a));

        // b will now be visible as it was not individually
        // set to be visible and the parent layer is now visible
        EXPECT_TRUE(!AzToolsFramework::IsEntitySetToBeLocked(b));
        EXPECT_TRUE(!AzToolsFramework::IsEntityLocked(b));

        // same story for e as for b
        EXPECT_TRUE(!AzToolsFramework::IsEntitySetToBeLocked(e));
        EXPECT_TRUE(!AzToolsFramework::IsEntityLocked(e));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // test to ensure the visibility flag on a layer entity is not modified
    // instead we rely on SetLayerChildrenVisibility and AreLayerChildrenVisible
    TEST_F(EditorEntityModelVisibilityFixture, LayerEntityVisibilityFlagIsNotModified)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        const AZ::EntityId a = CreateDefaultEditorEntity("A");
        const AZ::EntityId b = CreateDefaultEditorEntity("B");
        const AZ::EntityId c = CreateDefaultEditorEntity("C");

        m_layerId = CreateEditorLayerEntity("Layer1");

        AZ::TransformBus::Event(a, &AZ::TransformBus::Events::SetParent, m_layerId);
        AZ::TransformBus::Event(b, &AZ::TransformBus::Events::SetParent, m_layerId);
        AZ::TransformBus::Event(c, &AZ::TransformBus::Events::SetParent, m_layerId);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        AzToolsFramework::SetEntityVisibility(m_layerId, false);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(!AzToolsFramework::IsEntitySetToBeVisible(m_layerId));
        EXPECT_TRUE(!AzToolsFramework::IsEntityVisible(m_layerId));

        bool flagSetVisible = false;
        AzToolsFramework::EditorVisibilityRequestBus::EventResult(
            flagSetVisible, m_layerId, &AzToolsFramework::EditorVisibilityRequestBus::Events::GetVisibilityFlag);

        // even though a layer is set to not be visible, this is recorded by SetLayerChildrenVisibility
        // and AreLayerChildrenVisible - the visibility flag will not be modified and remains true
        EXPECT_TRUE(flagSetVisible);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    class EditorEntityInfoRequestActivateTestComponent : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            EditorEntityInfoRequestActivateTestComponent,
            "{849DA1FC-6A0C-4CB8-A0BB-D90DEE7FF7F7}",
            AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        // AZ::Component overrides ...
        void Activate() override
        {
            // ensure we can successfully read IsVisible and IsLocked (bus will be connected to in entity Init)
            AzToolsFramework::EditorEntityInfoRequestBus::EventResult(
                m_visible, GetEntityId(), &AzToolsFramework::EditorEntityInfoRequestBus::Events::IsVisible);
            AzToolsFramework::EditorEntityInfoRequestBus::EventResult(
                m_locked, GetEntityId(), &AzToolsFramework::EditorEntityInfoRequestBus::Events::IsLocked);
        }

        void Deactivate() override
        {
        }

        bool m_visible = false;
        bool m_locked = true;
    };

    void EditorEntityInfoRequestActivateTestComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorEntityInfoRequestActivateTestComponent>()->Version(0);
        }
    }

    class EditorEntityModelEntityInfoRequestFixture : public ToolsApplicationFixture
    {
    public:
        void SetUpEditorFixtureImpl() override
        {
            GetApplication()->RegisterComponentDescriptor(EditorEntityInfoRequestActivateTestComponent::CreateDescriptor());
        }
    };

    TEST_F(EditorEntityModelEntityInfoRequestFixture, EditorEntityInfoRequestBusRespondsInComponentActivate)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        AZ::Entity* entity = nullptr;
        CreateDefaultEditorEntity("Entity", &entity);

        entity->Deactivate();
        const auto* entityInfoComponent = entity->CreateComponent<EditorEntityInfoRequestActivateTestComponent>();

        // This is necessary to prevent a warning in the undo system.
        AzToolsFramework::ToolsApplicationRequests::Bus::Broadcast(
            &AzToolsFramework::ToolsApplicationRequests::Bus::Events::AddDirtyEntity, entity->GetId());
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        entity->Activate();
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(entityInfoComponent->m_visible);
        EXPECT_FALSE(entityInfoComponent->m_locked);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(EditorEntityModelEntityInfoRequestFixture, EditorEntityInfoRequestBusRespondsInComponentActivateInLayer)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        AZ::Entity* entity = nullptr;
        const AZ::EntityId entityId = CreateDefaultEditorEntity("Entity", &entity);
        const AZ::EntityId layerId = CreateEditorLayerEntity("Layer");

        AZ::TransformBus::Event(entityId, &AZ::TransformBus::Events::SetParent, layerId);

        AzToolsFramework::SetEntityVisibility(layerId, false);
        AzToolsFramework::SetEntityLockState(layerId, true);

        entity->Deactivate();
        auto* entityInfoComponent = entity->CreateComponent<EditorEntityInfoRequestActivateTestComponent>();

        // This is necessary to prevent a warning in the undo system.
        AzToolsFramework::ToolsApplicationRequests::Bus::Broadcast(
            &AzToolsFramework::ToolsApplicationRequests::Bus::Events::AddDirtyEntity, entity->GetId());
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // invert initial state to be sure we know Activate does what it's supposed to
        entityInfoComponent->m_visible = true;
        entityInfoComponent->m_locked = false;
        entity->Activate();
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_FALSE(entityInfoComponent->m_visible);
        EXPECT_TRUE(entityInfoComponent->m_locked);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST(HandleAccents, CurrentValidEntityIdBecomesHoveredWithNoSelectionAndUnstickySelect)
    {
        namespace azvi = AzToolsFramework::ViewportInteraction;

        const AZ::EntityId currentEntityId = AZ::EntityId(12345);
        AZ::EntityId hoveredEntityEntityId;

        AzToolsFramework::HandleAccentsContext handleAccentsContext;
        handleAccentsContext.m_ctrlHeld = false;
        handleAccentsContext.m_hasSelectedEntities = false;
        handleAccentsContext.m_usingBoxSelect = false;
        handleAccentsContext.m_usingStickySelect = false;

        bool currentEntityIdAccentAdded = false;
        AzToolsFramework::HandleAccents(
            currentEntityId, hoveredEntityEntityId, handleAccentsContext, azvi::MouseButtonsFromButton(azvi::MouseButton::None),
            [&currentEntityIdAccentAdded, currentEntityId](const AZ::EntityId entityId, const bool accent)
            {
                if (entityId == currentEntityId && accent)
                {
                    currentEntityIdAccentAdded = true;
                }
            });

        using ::testing::Eq;
        using ::testing::IsTrue;
        EXPECT_THAT(currentEntityId, Eq(hoveredEntityEntityId));
        EXPECT_THAT(currentEntityIdAccentAdded, IsTrue());
    }

    TEST(HandleAccents, CurrentValidEntityIdBecomesHoveredWithSelectionAndUnstickySelect)
    {
        namespace azvi = AzToolsFramework::ViewportInteraction;

        const AZ::EntityId currentEntityId = AZ::EntityId(12345);
        AZ::EntityId hoveredEntityEntityId;

        AzToolsFramework::HandleAccentsContext handleAccentsContext;
        handleAccentsContext.m_ctrlHeld = false;
        handleAccentsContext.m_hasSelectedEntities = true;
        handleAccentsContext.m_usingBoxSelect = false;
        handleAccentsContext.m_usingStickySelect = false;

        bool currentEntityIdAccentAdded = false;
        AzToolsFramework::HandleAccents(
            currentEntityId, hoveredEntityEntityId, handleAccentsContext, azvi::MouseButtonsFromButton(azvi::MouseButton::None),
            [&currentEntityIdAccentAdded, currentEntityId](const AZ::EntityId entityId, const bool accent)
            {
                if (entityId == currentEntityId && accent)
                {
                    currentEntityIdAccentAdded = true;
                }
            });

        using ::testing::Eq;
        using ::testing::IsTrue;
        EXPECT_THAT(currentEntityId, Eq(hoveredEntityEntityId));
        EXPECT_THAT(currentEntityIdAccentAdded, IsTrue());
    }

    TEST(HandleAccents, CurrentValidEntityIdDoesNotBecomeHoveredWithSelectionUnstickySelectAndInvalidButton)
    {
        namespace azvi = AzToolsFramework::ViewportInteraction;

        const AZ::EntityId currentEntityId = AZ::EntityId(12345);
        AZ::EntityId hoveredEntityEntityId = AZ::EntityId(54321);

        AzToolsFramework::HandleAccentsContext handleAccentsContext;
        handleAccentsContext.m_ctrlHeld = false;
        handleAccentsContext.m_hasSelectedEntities = false;
        handleAccentsContext.m_usingBoxSelect = false;
        handleAccentsContext.m_usingStickySelect = false;

        bool hoveredEntityIdAccentRemoved = false;
        AzToolsFramework::HandleAccents(
            currentEntityId, hoveredEntityEntityId, handleAccentsContext, azvi::MouseButtonsFromButton(azvi::MouseButton::Middle),
            [&hoveredEntityIdAccentRemoved, hoveredEntityEntityId](const AZ::EntityId entityId, const bool accent)
            {
                if (entityId == hoveredEntityEntityId && !accent)
                {
                    hoveredEntityIdAccentRemoved = true;
                }
            });

        using ::testing::Eq;
        using ::testing::IsFalse;
        using ::testing::IsTrue;
        EXPECT_THAT(hoveredEntityEntityId.IsValid(), IsFalse());
        EXPECT_THAT(hoveredEntityIdAccentRemoved, IsTrue());
    }

    TEST(HandleAccents, CurrentValidEntityIdDoesNotBecomeHoveredWithSelectionUnstickySelectAndDoingBoxSelect)
    {
        namespace azvi = AzToolsFramework::ViewportInteraction;

        const AZ::EntityId currentEntityId = AZ::EntityId(12345);
        AZ::EntityId hoveredEntityEntityId = AZ::EntityId(54321);

        AzToolsFramework::HandleAccentsContext handleAccentsContext;
        handleAccentsContext.m_ctrlHeld = false;
        handleAccentsContext.m_hasSelectedEntities = false;
        handleAccentsContext.m_usingBoxSelect = true;
        handleAccentsContext.m_usingStickySelect = false;

        bool hoveredEntityIdAccentRemoved = false;
        AzToolsFramework::HandleAccents(
            currentEntityId, hoveredEntityEntityId, handleAccentsContext, azvi::MouseButtonsFromButton(azvi::MouseButton::None),
            [&hoveredEntityIdAccentRemoved, hoveredEntityEntityId](const AZ::EntityId entityId, const bool accent)
            {
                if (entityId == hoveredEntityEntityId && !accent)
                {
                    hoveredEntityIdAccentRemoved = true;
                }
            });

        using ::testing::Eq;
        using ::testing::IsFalse;
        using ::testing::IsTrue;
        EXPECT_THAT(hoveredEntityEntityId.IsValid(), IsFalse());
        EXPECT_THAT(hoveredEntityIdAccentRemoved, IsTrue());
    }

    // mimics the mouse moving off of hovered entity onto a new entity with sticky select enabled
    TEST(HandleAccents, CurrentValidEntityIdDoesNotBecomeHoveredWithSelectionAndStickySelect)
    {
        namespace azvi = AzToolsFramework::ViewportInteraction;

        const AZ::EntityId currentEntityId = AZ::EntityId(12345);
        AZ::EntityId hoveredEntityEntityId = AZ::EntityId(54321);

        AzToolsFramework::HandleAccentsContext handleAccentsContext;
        handleAccentsContext.m_ctrlHeld = false;
        handleAccentsContext.m_hasSelectedEntities = true;
        handleAccentsContext.m_usingBoxSelect = false;
        handleAccentsContext.m_usingStickySelect = true;

        bool hoveredEntityIdAccentRemoved = false;
        AzToolsFramework::HandleAccents(
            currentEntityId, hoveredEntityEntityId, handleAccentsContext, azvi::MouseButtonsFromButton(azvi::MouseButton::None),
            [&hoveredEntityIdAccentRemoved, hoveredEntityEntityId](const AZ::EntityId entityId, const bool accent)
            {
                if (entityId == hoveredEntityEntityId && !accent)
                {
                    hoveredEntityIdAccentRemoved = true;
                }
            });

        using ::testing::Eq;
        using ::testing::IsFalse;
        using ::testing::IsTrue;
        EXPECT_THAT(hoveredEntityIdAccentRemoved, IsTrue());
        EXPECT_THAT(hoveredEntityEntityId.IsValid(), IsFalse());
    }

    TEST(HandleAccents, CurrentValidEntityIdDoesBecomeHoveredWithSelectionAndStickySelectAndCtrl)
    {
        namespace azvi = AzToolsFramework::ViewportInteraction;

        const AZ::EntityId currentEntityId = AZ::EntityId(12345);
        AZ::EntityId hoveredEntityEntityId = AZ::EntityId(54321);

        AzToolsFramework::HandleAccentsContext handleAccentsContext;
        handleAccentsContext.m_ctrlHeld = true;
        handleAccentsContext.m_hasSelectedEntities = true;
        handleAccentsContext.m_usingBoxSelect = false;
        handleAccentsContext.m_usingStickySelect = true;

        bool currentEntityIdAccentAdded = false;
        bool hoveredEntityIdAccentRemoved = false;
        AzToolsFramework::HandleAccents(
            currentEntityId, hoveredEntityEntityId, handleAccentsContext, azvi::MouseButtonsFromButton(azvi::MouseButton::None),
            [&hoveredEntityIdAccentRemoved, &currentEntityIdAccentAdded, currentEntityId,
             hoveredEntityEntityId](const AZ::EntityId entityId, const bool accent)
            {
                if (entityId == currentEntityId && accent)
                {
                    currentEntityIdAccentAdded = true;
                }

                if (entityId == hoveredEntityEntityId && !accent)
                {
                    hoveredEntityIdAccentRemoved = true;
                }
            });

        using ::testing::Eq;
        using ::testing::IsFalse;
        using ::testing::IsTrue;
        EXPECT_THAT(currentEntityIdAccentAdded, IsTrue());
        EXPECT_THAT(hoveredEntityIdAccentRemoved, IsTrue());
        EXPECT_THAT(hoveredEntityEntityId, Eq(AZ::EntityId(12345)));
    }

    class EditorTransformComponentSelectionRenderGeometryIntersectionFixture : public ToolsApplicationFixture
    {
    public:
        void SetUpEditorFixtureImpl() override
        {
            auto* app = GetApplication();
            // register a simple component implementing BoundsRequestBus and EditorComponentSelectionRequestsBus
            app->RegisterComponentDescriptor(BoundsTestComponent::CreateDescriptor());
            // register a component implementing RenderGeometry::IntersectionRequestBus
            app->RegisterComponentDescriptor(RenderGeometryIntersectionTestComponent::CreateDescriptor());

            auto createEntityWithGeometryIntersectionFn = [](const char* entityName)
            {
                AZ::Entity* entity = nullptr;
                AZ::EntityId entityId = CreateDefaultEditorEntity(entityName, &entity);

                entity->Deactivate();
                entity->CreateComponent<RenderGeometryIntersectionTestComponent>();
                entity->Activate();

                return entityId;
            };

            m_entityIdGround = createEntityWithGeometryIntersectionFn("Entity1");
            m_entityIdBox = createEntityWithGeometryIntersectionFn("Entity2");

            if (auto* ground = AzToolsFramework::GetEntityById(m_entityIdGround)->FindComponent<RenderGeometryIntersectionTestComponent>())
            {
                ground->m_localBounds = AZ::Aabb::CreateFromMinMax(AZ::Vector3(-10.0f, -10.0f, -0.5f), AZ::Vector3(10.0f, 10.0f, 0.5f));
            }

            AzToolsFramework::SetWorldTransform(m_entityIdGround, AZ::Transform::CreateTranslation(AZ::Vector3(0.0f, 10.0f, 5.0f)));

            if (auto* box = AzToolsFramework::GetEntityById(m_entityIdBox)->FindComponent<RenderGeometryIntersectionTestComponent>())
            {
                box->m_localBounds = AZ::Aabb::CreateFromMinMax(AZ::Vector3(-0.5f), AZ::Vector3(0.5f));
            }

            AzToolsFramework::SetWorldTransform(
                m_entityIdBox,
                AZ::Transform::CreateFromMatrix3x3AndTranslation(
                    AZ::Matrix3x3::CreateRotationZ(AZ::DegToRad(45.0f)), AZ::Vector3(0.0f, 10.0f, 7.0f)));
        }

        AZ::EntityId m_entityIdGround;
        AZ::EntityId m_entityIdBox;
    };

    using EditorTransformComponentSelectionRenderGeometryIntersectionManipulatorFixture =
        IndirectCallManipulatorViewportInteractionFixtureMixin<EditorTransformComponentSelectionRenderGeometryIntersectionFixture>;

    TEST_F(
        EditorTransformComponentSelectionRenderGeometryIntersectionManipulatorFixture, BoxCanBePlacedOnMeshSurfaceUsingSurfaceManipulator)
    {
        // camera (go to position format) - 0.00, 20.00, 12.00, -35.00, -180.00
        m_cameraState.m_viewportSize = AZ::Vector2(1280.0f, 720.0f);
        AzFramework::SetCameraTransform(
            m_cameraState,
            AZ::Transform::CreateFromMatrix3x3AndTranslation(
                AZ::Matrix3x3::CreateRotationZ(AZ::DegToRad(-180.0f)) * AZ::Matrix3x3::CreateRotationX(AZ::DegToRad(-35.0f)),
                AZ::Vector3(0.0f, 20.0f, 12.0f)));

        // the initial starting position of the entity
        const auto initialTransformWorld = AzToolsFramework::GetWorldTransform(m_entityIdBox);
        // where the entity should end up (snapped to the larger ground surface)
        const auto finalTransformWorld =
            AZ::Transform::CreateFromQuaternionAndTranslation(initialTransformWorld.GetRotation(), AZ::Vector3(2.5f, 12.5f, 5.5f));

        // calculate the position in screen space of the initial position of the entity
        const auto initialPositionScreen = AzFramework::WorldToScreen(initialTransformWorld.GetTranslation(), m_cameraState);
        // calculate the position in screen space of the final position of the entity
        const auto finalPositionScreen = AzFramework::WorldToScreen(finalTransformWorld.GetTranslation(), m_cameraState);

        // select the entity (this will cause the manipulators to appear in EditorTransformComponentSelection)
        AzToolsFramework::SelectEntity(m_entityIdBox);

        // press and drag the mouse (starting where the surface manipulator is)
        m_actionDispatcher->CameraState(m_cameraState)
            ->MousePosition(initialPositionScreen)
            ->MouseLButtonDown()
            ->MousePosition(finalPositionScreen)
            ->MouseLButtonUp();

        // read back the position of the entity now
        const AZ::Transform finalEntityTransform = AzToolsFramework::GetWorldTransform(m_entityIdBox);

        // ensure final world positions match
        EXPECT_THAT(finalEntityTransform, IsCloseTolerance(finalTransformWorld, 0.01f));
    }

    TEST_F(
        EditorTransformComponentSelectionRenderGeometryIntersectionManipulatorFixture,
        SurfaceManipulatorFollowsMouseAtDefaultEditorDistanceFromCameraWhenNoMeshIntersection)
    {
        // camera (go to position format) - 0.00, 25.00, 12.00, 0.00, -180.00
        m_cameraState.m_viewportSize = AZ::Vector2(1280.0f, 720.0f);
        AzFramework::SetCameraTransform(
            m_cameraState,
            AZ::Transform::CreateFromMatrix3x3AndTranslation(
                AZ::Matrix3x3::CreateRotationZ(AZ::DegToRad(-180.0f)), AZ::Vector3(0.0f, 25.0f, 12.0f)));

        // the initial starting position of the entity
        const auto initialTransformWorld = AzToolsFramework::GetWorldTransform(m_entityIdBox);
        // where the entity should end up (default distance away from the camera/near clip under where the mouse is)
        const auto finalTransformWorld =
            AZ::Transform::CreateFromQuaternionAndTranslation(initialTransformWorld.GetRotation(), AZ::Vector3(0.0f, 14.9f, 12.0f));

        // calculate the position in screen space of the initial position of the entity
        const auto initialPositionScreen = AzFramework::WorldToScreen(initialTransformWorld.GetTranslation(), m_cameraState);
        // calculate the position in screen space of the final position of the entity
        const auto finalPositionScreen = AzFramework::WorldToScreen(finalTransformWorld.GetTranslation(), m_cameraState);

        // select the entity (this will cause the manipulators to appear in EditorTransformComponentSelection)
        AzToolsFramework::SelectEntity(m_entityIdBox);

        // press and drag the mouse (starting where the surface manipulator is)
        m_actionDispatcher->CameraState(m_cameraState)
            ->MousePosition(initialPositionScreen)
            ->MouseLButtonDown()
            ->MousePosition(finalPositionScreen)
            ->MouseLButtonUp();

        // read back the position of the entity now
        const AZ::Transform finalEntityTransform = AzToolsFramework::GetWorldTransform(m_entityIdBox);

        const auto viewportRay = AzToolsFramework::ViewportInteraction::ViewportScreenToWorldRay(m_cameraState, initialPositionScreen);
        const auto distanceAway = (finalEntityTransform.GetTranslation() - viewportRay.m_origin).GetLength();

        // ensure final world positions match
        EXPECT_THAT(finalEntityTransform, IsCloseTolerance(finalTransformWorld, 0.01f));
        // ensure distance away is what we expect
        EXPECT_NEAR(distanceAway, AzToolsFramework::GetDefaultEntityPlacementDistance(), 0.001f);
    }

    TEST_F(
        EditorTransformComponentSelectionRenderGeometryIntersectionManipulatorFixture,
        MiddleMouseButtonWithShiftAndCtrlHeldOnMeshSurfaceWillSnapSelectedEntityToIntersectionPoint)
    {
        // camera (go to position format) - 21.00, 8.00, 11.00, -22.00, 150.00
        m_cameraState.m_viewportSize = AZ::Vector2(1280.0f, 720.0f);
        AzFramework::SetCameraTransform(
            m_cameraState,
            AZ::Transform::CreateFromMatrix3x3AndTranslation(
                AZ::Matrix3x3::CreateRotationZ(AZ::DegToRad(150.0f)) * AZ::Matrix3x3::CreateRotationX(AZ::DegToRad(-22.0f)),
                AZ::Vector3(21.0f, 8.0f, 11.0f)));

        // position the ground entity
        AzToolsFramework::SetWorldTransform(
            m_entityIdGround,
            AZ::Transform::CreateFromMatrix3x3AndTranslation(
                AZ::Matrix3x3::CreateRotationY(AZ::DegToRad(40.0f)) * AZ::Matrix3x3::CreateRotationZ(AZ::DegToRad(60.0f)),
                AZ::Vector3(14.0f, -6.0f, 5.0f)));

        // select the other entity (a 1x1x1 box)
        AzToolsFramework::SelectEntity(m_entityIdBox);

        // expected world position (value taken from editor scenario)
        const auto expectedWorldPosition = AZ::Vector3(13.606657f, -2.6753534f, 5.9827675f);
        const auto screenPosition = AzFramework::WorldToScreen(expectedWorldPosition, m_cameraState);

        // perform snap action
        m_actionDispatcher->CameraState(m_cameraState)
            ->MousePosition(screenPosition)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Control)
            ->KeyboardModifierDown(AzToolsFramework::ViewportInteraction::KeyboardModifier::Shift)
            ->MouseMButtonDown();

        // read back the current entity transform after placement
        const AZ::Transform finalEntityTransform = AzToolsFramework::GetWorldTransform(m_entityIdBox);
        EXPECT_THAT(finalEntityTransform.GetTranslation(), IsCloseTolerance(expectedWorldPosition, 0.01f));
    }
} // namespace UnitTest
