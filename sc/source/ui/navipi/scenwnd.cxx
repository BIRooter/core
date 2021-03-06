/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <sfx2/bindings.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/viewfrm.hxx>
#include <svl/slstitm.hxx>
#include <svl/stritem.hxx>
#include <vcl/commandevent.hxx>
#include <vcl/event.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld.hxx>
#include <vcl/settings.hxx>
#include <navipi.hxx>
#include <sc.hrc>
#include <globstr.hrc>
#include <scresid.hxx>
#include <helpids.h>
#include <global.hxx>

// class ScScenarioWindow ------------------------------------------------

ScScenarioListBox::ScScenarioListBox( ScScenarioWindow& rParent ) :
    ListBox( &rParent, WB_BORDER | WB_TABSTOP ),
    mrParent( rParent )
{
    vcl::Font aFont( GetFont() );
    aFont.SetTransparent( true );
    aFont.SetWeight( WEIGHT_LIGHT );
    SetFont( aFont );
}

ScScenarioListBox::~ScScenarioListBox()
{
}

void ScScenarioListBox::UpdateEntries( const std::vector<OUString> &aNewEntryList )
{
    Clear();
    maEntries.clear();

    switch( aNewEntryList.size() )
    {
        case 0:
            // no scenarios in current sheet
            mrParent.SetComment( EMPTY_OUSTRING );
        break;

        case 1:
            // sheet is a scenario container, comment only
            mrParent.SetComment( aNewEntryList[0] );
        break;

        default:
        {
            // sheet contains scenarios
            assert(aNewEntryList.size() % 3 == 0 && "ScScenarioListBox::UpdateEntries - wrong list size");
            SetUpdateMode( false );

            std::vector<OUString>::const_iterator iter;
            for (iter = aNewEntryList.begin(); iter != aNewEntryList.end(); ++iter)
            {
                ScenarioEntry aEntry;

                // first entry of a triple is the scenario name
                aEntry.maName = *iter;

                // second entry of a triple is the scenario comment
                ++iter;
                aEntry.maComment = *iter;

                // third entry of a triple is the protection ("0" = not protected, "1" = protected)
                ++iter;
                aEntry.mbProtected = !(*iter).isEmpty() && (*iter)[0] != '0';

                maEntries.push_back( aEntry );
                InsertEntry( aEntry.maName );
            }
            SetUpdateMode( true );
            SetNoSelection();
            mrParent.SetComment( EMPTY_OUSTRING );
        }
    }
}

void ScScenarioListBox::Select()
{
    if( const ScenarioEntry* pEntry = GetSelectedScenarioEntry() )
        mrParent.SetComment( pEntry->maComment );
}

void ScScenarioListBox::DoubleClick()
{
    SelectScenario();
}

bool ScScenarioListBox::EventNotify( NotifyEvent& rNEvt )
{
    bool bHandled = false;

    if( rNEvt.GetType() == MouseNotifyEvent::KEYINPUT )
    {
        vcl::KeyCode aCode = rNEvt.GetKeyEvent()->GetKeyCode();
        switch( aCode.GetCode() )
        {
            case KEY_RETURN:
                SelectScenario();
                bHandled = true;
            break;
            case KEY_DELETE:
                DeleteScenario();
                bHandled = true;
            break;
        }
    }
    else if ( rNEvt.GetType() == MouseNotifyEvent::COMMAND && GetSelectedEntryCount() )
    {
        const CommandEvent* pCEvt = rNEvt.GetCommandEvent();
        if ( pCEvt && pCEvt->GetCommand() == CommandEventId::ContextMenu )
        {
            if( const ScenarioEntry* pEntry = GetSelectedScenarioEntry() )
            {
                if( !pEntry->mbProtected )
                {
                    VclBuilder aBuilder(nullptr, VclBuilderContainer::getUIRootDir(), "modules/scalc/ui/scenariomenu.ui", "");
                    VclPtr<PopupMenu> aPopup(aBuilder.get_menu("menu"));
                    sal_uInt16 nId = aPopup->Execute(this, pCEvt->GetMousePosPixel());
                    OString sIdent(aPopup->GetItemIdent(nId));
                    if (sIdent == "delete")
                        DeleteScenario();
                    else if (sIdent == "edit")
                        EditScenario();
                }
            }
            bHandled = true;
        }
    }

    return bHandled || ListBox::EventNotify(rNEvt);
}

const ScScenarioListBox::ScenarioEntry* ScScenarioListBox::GetSelectedScenarioEntry() const
{
    size_t nPos = GetSelectedEntryPos();
    return (nPos < maEntries.size()) ? &maEntries[ nPos ] : nullptr;
}

void ScScenarioListBox::ExecuteScenarioSlot( sal_uInt16 nSlotId )
{
    if( SfxViewFrame* pViewFrm = SfxViewFrame::Current() )
    {
        SfxStringItem aStringItem( nSlotId, GetSelectedEntry() );
        pViewFrm->GetDispatcher()->ExecuteList(nSlotId,
                SfxCallMode::SLOT | SfxCallMode::RECORD, { &aStringItem } );
    }
}

void ScScenarioListBox::SelectScenario()
{
    if( GetSelectedEntryCount() > 0 )
        ExecuteScenarioSlot( SID_SELECT_SCENARIO );
}

void ScScenarioListBox::EditScenario()
{
    if( GetSelectedEntryCount() > 0 )
        ExecuteScenarioSlot( SID_EDIT_SCENARIO );
}

void ScScenarioListBox::DeleteScenario()
{
    if( GetSelectedEntryCount() > 0 )
    {
        std::unique_ptr<weld::MessageDialog> xQueryBox(Application::CreateMessageDialog(nullptr,
                                                       VclMessageType::Question, VclButtonsType::YesNo,
                                                       ScResId(STR_QUERY_DELSCENARIO)));
        xQueryBox->set_default_response(RET_YES);
        if (xQueryBox->run() == RET_YES)
            ExecuteScenarioSlot( SID_DELETE_SCENARIO );
    }
}

// class ScScenarioWindow ------------------------------------------------

ScScenarioWindow::ScScenarioWindow( vcl::Window* pParent, const OUString& aQH_List,
                                    const OUString& aQH_Comment)
    :   Window      ( pParent, WB_TABSTOP | WB_DIALOGCONTROL ),
        aLbScenario ( VclPtr<ScScenarioListBox>::Create(*this) ),
        aEdComment  ( VclPtr<VclMultiLineEdit>::Create(this,  WB_BORDER | WB_LEFT | WB_READONLY | WB_VSCROLL | WB_TABSTOP) )
{
    vcl::Font aFont( GetFont() );
    aFont.SetTransparent( true );
    aFont.SetWeight( WEIGHT_LIGHT );
    aEdComment->SetFont( aFont );
    aEdComment->SetMaxTextLen( 512 );
    aLbScenario->SetPosPixel( Point(0,0) );
    aLbScenario->SetHelpId(HID_SC_SCENWIN_TOP);
    aEdComment->SetHelpId(HID_SC_SCENWIN_BOTTOM);
    aLbScenario->Show();
    aEdComment->Show();

    aLbScenario->SetQuickHelpText(aQH_List);
    aEdComment->SetQuickHelpText(aQH_Comment);
    aEdComment->SetBackground( COL_LIGHTGRAY );

    SfxViewFrame* pViewFrm = SfxViewFrame::Current();
    if (pViewFrm)
    {
        SfxBindings& rBindings = pViewFrm->GetBindings();
        rBindings.Invalidate( SID_SELECT_SCENARIO );
        rBindings.Update( SID_SELECT_SCENARIO );
    }
}

ScScenarioWindow::~ScScenarioWindow()
{
    disposeOnce();
}

void ScScenarioWindow::dispose()
{
    aLbScenario.disposeAndClear();
    aEdComment.disposeAndClear();
    vcl::Window::dispose();
}

void ScScenarioWindow::Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect)
{
    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    Color aBgColor = rStyleSettings.GetFaceColor();

    SetBackground( aBgColor );

    Window::Paint(rRenderContext, rRect);
}

void ScScenarioWindow::NotifyState( const SfxPoolItem* pState )
{
    if( pState )
    {
        aLbScenario->Enable();

        if ( auto pStringItem = dynamic_cast<const SfxStringItem*>( pState) )
        {
            const OUString& aNewEntry( pStringItem->GetValue() );

            if ( !aNewEntry.isEmpty() )
                aLbScenario->SelectEntry( aNewEntry );
            else
                aLbScenario->SetNoSelection();
        }
        else if ( auto pStringListItem = dynamic_cast<const SfxStringListItem*>( pState) )
        {
            aLbScenario->UpdateEntries( pStringListItem->GetList() );
        }
    }
    else
    {
        aLbScenario->Disable();
        aLbScenario->SetNoSelection();
    }
}

void ScScenarioWindow::Resize()
{
    Window::Resize();

    Size aSize(GetSizePixel());
    long nHeight = aSize.Height() / 2;

    aSize.setHeight( nHeight );
    aLbScenario->SetSizePixel(aSize);

    aSize.AdjustHeight( -4 );
    aEdComment->SetPosSizePixel(Point(0, nHeight + 4), aSize);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
