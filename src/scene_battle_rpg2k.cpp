/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <sstream>
#include "input.h"
#include "player.h"
#include "sprite.h"
#include "game_battler.h"
#include "game_system.h"
#include "game_party.h"
#include "game_enemy.h"
#include "game_enemyparty.h"
#include "game_message.h"
#include "game_battle.h"
#include "game_battlealgorithm.h"
#include "game_screen.h"
#include "battle_animation.h"
#include <lcf/reader_util.h>
#include "scene_battle_rpg2k.h"
#include "scene_battle.h"
#include "scene_gameover.h"
#include "output.h"
#include "rand.h"
#include "autobattle.h"
#include "enemyai.h"

Scene_Battle_Rpg2k::Scene_Battle_Rpg2k(const BattleArgs& args) :
	Scene_Battle(args)
{
}

Scene_Battle_Rpg2k::~Scene_Battle_Rpg2k() {
}

void Scene_Battle_Rpg2k::Update() {
	battle_message_window->Update();

	if (message_window->IsVisible() && !message_box_got_visible) {
		battle_message_window->Clear();
		message_box_got_visible = true;
	} else if (!message_window->IsVisible() && message_box_got_visible) {
		message_box_got_visible = false;
	}

	Scene_Battle::Update();
}

void Scene_Battle_Rpg2k::CreateUi() {
	Scene_Battle::CreateUi();

	CreateBattleTargetWindow();
	CreateBattleCommandWindow();

	battle_message_window.reset(new Window_BattleMessage(0, (SCREEN_TARGET_HEIGHT - 80), SCREEN_TARGET_WIDTH, 80));

	if (!IsEscapeAllowed()) {
		options_window->DisableItem(2);
	}
}

void Scene_Battle_Rpg2k::CreateBattleTargetWindow() {
	std::vector<std::string> commands;
	std::vector<Game_Battler*> enemies;
	Main_Data::game_enemyparty->GetActiveBattlers(enemies);

	for (auto& enemy: enemies) {
		commands.push_back(ToString(enemy->GetName()));
	}

	target_window.reset(new Window_Command(commands, 136, 4));
	target_window->SetHeight(80);
	target_window->SetY(SCREEN_TARGET_HEIGHT-80);
	// Above other windows
	target_window->SetZ(Priority_Window + 10);
}

void Scene_Battle_Rpg2k::CreateBattleCommandWindow() {
	std::vector<std::string> commands;
	commands.push_back(ToString(lcf::Data::terms.command_attack));
	commands.push_back(ToString(lcf::Data::terms.command_skill));
	commands.push_back(ToString(lcf::Data::terms.command_defend));
	commands.push_back(ToString(lcf::Data::terms.command_item));

	command_window.reset(new Window_Command(commands, 76));
	command_window->SetHeight(80);
	command_window->SetX(SCREEN_TARGET_WIDTH - option_command_mov);
	command_window->SetY(SCREEN_TARGET_HEIGHT-80);
}

void Scene_Battle_Rpg2k::RefreshCommandWindow() {
	command_window->SetItemText(1, active_actor->GetSkillName());
}

void Scene_Battle_Rpg2k::SetState(Scene_Battle::State new_state) {
	previous_state = state;
	state = new_state;

	SetBattleActionSubState(0);

	options_window->SetActive(false);
	status_window->SetActive(false);
	command_window->SetActive(false);
	item_window->SetActive(false);
	skill_window->SetActive(false);
	target_window->SetActive(false);
	battle_message_window->SetActive(false);

	switch (state) {
	case State_Start:
		battle_message_window->SetActive(true);
		break;
	case State_SelectOption:
		options_window->SetActive(true);
		break;
	case State_SelectActor:
		status_window->SetActive(true);
		break;
	case State_AutoBattle:
		break;
	case State_SelectCommand:
		command_window->SetActive(true);
		RefreshCommandWindow();
		break;
	case State_SelectEnemyTarget:
		select_target_flash_count = 0;
		break;
	case State_SelectAllyTarget:
		status_window->SetActive(true);
		break;
	case State_Battle:
		// no-op
		break;
	case State_SelectItem:
		item_window->SetActive(true);
		item_window->SetActor(active_actor);
		item_window->Refresh();
		break;
	case State_SelectSkill:
		skill_window->SetActive(true);
		skill_window->SetActor(active_actor->GetId());
		if (previous_state == State_SelectCommand)
			skill_window->RestoreActorIndex(actor_index - 1);
		break;
	case State_Victory:
	case State_Defeat:
		battle_message_window->Clear();
		break;
	case State_Escape:
		battle_message_window->SetActive(true);
		break;
	}

	if (state != State_SelectEnemyTarget && state != State_SelectAllyTarget) {
		options_window->SetVisible(false);
		status_window->SetVisible(false);
		command_window->SetVisible(false);
		target_window->SetVisible(false);
		battle_message_window->SetVisible(false);
		item_window->SetVisible(false);
		skill_window->SetVisible(false);
		help_window->SetVisible(false);

	}

	if (previous_state == State_SelectSkill) {
		skill_window->SaveActorIndex(actor_index - 1);
	}

	switch (state) {
	case State_Start:
		battle_message_window->SetVisible(true);
		break;
	case State_SelectOption:
		options_window->SetVisible(true);
		options_window->SetX(0);
		status_window->SetVisible(true);
		status_window->SetX(option_command_mov);
		status_window->SetIndex(-1);
		command_window->SetX(SCREEN_TARGET_WIDTH);
		command_window->SetIndex(-1);
		status_window->Refresh();
		move_screen = true;
		break;
	case State_SelectActor:
		SelectNextActor();
		break;
	case State_AutoBattle:
		SetState(State_SelectActor);
		break;
	case State_SelectCommand:
		options_window->SetX(-option_command_mov);
		status_window->SetVisible(true);
		status_window->SetX(0);
		command_window->SetVisible(true);
		command_window->SetX(SCREEN_TARGET_WIDTH - option_command_mov);
		if (previous_state == State_SelectActor) {
			command_window->SetIndex(0);
		}
		break;
	case State_SelectEnemyTarget:
		target_window->SetActive(true);
		target_window->SetVisible(true);
		target_window->SetIndex(0);
		break;
	case State_SelectAllyTarget:
		status_window->SetVisible(true);
		status_window->SetX(0);
		status_window->SetIndex(0);
		break;
	case State_Battle:
		battle_message_window->SetVisible(true);
		break;
	case State_SelectItem:
		item_window->SetVisible(true);
		item_window->SetHelpWindow(help_window.get());
		help_window->SetVisible(true);
		break;
	case State_SelectSkill:
		skill_window->SetVisible(true);
		skill_window->SetHelpWindow(help_window.get());
		help_window->SetVisible(true);
		break;
	case State_Victory:
	case State_Defeat:
		battle_message_window->SetVisible(true);
		break;
	case State_Escape:
		battle_message_window->SetVisible(true);
		break;
	}

	// If SelectOption <-> SelectCommand => Display Movement:
	if (state == State_SelectOption && previous_state == State_SelectCommand) {
		options_window->InitMovement(options_window->GetX() - option_command_mov, options_window->GetY(),
			options_window->GetX(), options_window->GetY(), option_command_time);

		status_window->InitMovement(status_window->GetX() - option_command_mov, status_window->GetY(),
			status_window->GetX(), status_window->GetY(), option_command_time);

		command_window->SetVisible(true);
		command_window->InitMovement(command_window->GetX() - option_command_mov, command_window->GetY(),
			command_window->GetX(), command_window->GetY(), option_command_time);
	}
	else if (state == State_SelectCommand && move_screen) {
		move_screen = false;
		options_window->SetVisible(true);
		options_window->InitMovement(options_window->GetX() + option_command_mov, options_window->GetY(),
			options_window->GetX(), options_window->GetY(), option_command_time);

		status_window->InitMovement(status_window->GetX() + option_command_mov, status_window->GetY(),
			status_window->GetX(), status_window->GetY(), option_command_time);

		command_window->InitMovement(command_window->GetX() + option_command_mov, command_window->GetY(),
			command_window->GetX(), command_window->GetY(), option_command_time);
	}
}

void Scene_Battle_Rpg2k::NextTurn() {
	Scene_Battle::NextTurn(nullptr);

	auto_battle = false;
}

void Scene_Battle_Rpg2k::ProcessActions() {
	switch (state) {
	case State_Start:
		if (DisplayMonstersInMessageWindow()) {
			Game_Battle::RefreshEvents();
			SetState(State_Battle);
		}
		break;
	case State_SelectOption:
		// No Auto battle/Escape when all actors are sleeping or similar
		if (!Main_Data::game_party->IsAnyControllable()) {
			SelectNextActor();
		}
		break;
	case State_SelectActor:
	case State_AutoBattle:
		CheckResultConditions();

		if (help_window->IsVisible() && message_timer > 0) {
			message_timer--;
			if (message_timer <= 0)
				help_window->SetVisible(false);
		}

		break;
	case State_Battle:
		if (!battle_action_pending) {
			// If no battle action is running, we need to check for battle events which could have
			// triggered win/loss.
			if (CheckResultConditions()) {
				return;
			}
			// Don't remove actions until the current action is done.
			RemoveActionsForNonExistantBattlers();
		}
		if (!battle_actions.empty()) {
			auto* battler = battle_actions.front();
			if (!battle_action_pending) {
				// If we will start a new battle action, first check for state changes
				// such as death, paralyze, confuse, etc..
				PrepareBattleAction(battler);
			}

			auto* alg = battler->GetBattleAlgorithm().get();
			if (alg == nullptr) {
				Output::Warning("ProcessActions: Invalid action for battler {} ({})", battler->GetId(), battler->GetName());
				Output::Warning("Please report a bug!");
			}

			if (ProcessBattleAction(alg)) {
				battle_action_pending = false;
				RemoveCurrentAction();
				battle_message_window->Clear();
				Game_Battle::RefreshEvents();

				if (CheckResultConditions()) {
					return;
				}
			}
		} else {
			// Everybody acted
			actor_index = 0;

			if (Game_Battle::GetTurn() > 0) {
				first_strike = false;
			}

			// Go right into next turn if no actors controllable.
			if (!Main_Data::game_party->IsAnyControllable()) {
				SelectNextActor();
			} else {
				SetState(State_SelectOption);
			}
		}
		break;
	case State_SelectEnemyTarget: {
		std::vector<Game_Battler*> enemies;
		Main_Data::game_enemyparty->GetActiveBattlers(enemies);

		Game_Enemy* target = static_cast<Game_Enemy*>(enemies[target_window->GetIndex()]);
		++select_target_flash_count;

		if (select_target_flash_count == 60) {
			SelectionFlash(target);
			select_target_flash_count = 0;
		}
		break;
	}
	case State_Victory:
		EndBattle(BattleResult::Victory);
		break;
	case State_Defeat:
		EndBattle(BattleResult::Defeat);
		break;
	case State_Escape:
		Escape();
		break;
	default:
		break;
	}
}

void Scene_Battle_Rpg2k::SetBattleActionState(BattleActionState state) {
	battle_action_state = state;
	battle_action_substate = 0;
}

void Scene_Battle_Rpg2k::SetBattleActionSubState(int substate, bool reset_index) {
	battle_action_substate = substate;
	if (reset_index) {
		battle_action_substate_index = 0;
	}
}

bool Scene_Battle_Rpg2k::ProcessNextAction(BattleActionState state, Game_BattleAlgorithm::AlgorithmBase* action) {
    SetBattleActionState(state);
    return ProcessBattleAction(action);
}

bool Scene_Battle_Rpg2k::ProcessNextSubState(int substate, Game_BattleAlgorithm::AlgorithmBase* action, bool reset_index) {
	SetBattleActionSubState(substate, reset_index);
	return ProcessBattleAction(action);
}

bool Scene_Battle_Rpg2k::ProcessBattleAction(Game_BattleAlgorithm::AlgorithmBase* action) {
	if (action == nullptr) {
		Output::Warning("ProcessBattleAction: Invalid battle action");
		Output::Warning("Please report a bug!");
		return true;
	}

	if (!battle_action_pending) {
		// First time we are called, do initialization.
		battle_action_wait = 0;
		SetBattleActionState(BattleActionState_Begin);
		battle_action_start_index = 0;
		battle_action_results_index = 0;

		battle_action_pending = true;
	}

	const bool wait = !CheckWait();

	if (wait) {
		return false;
	}

	switch (battle_action_state) {
		case BattleActionState_Begin:
			return ProcessActionBegin(action);
		case BattleActionState_Usage1:
			return ProcessActionUsage1(action);
		case BattleActionState_Usage2:
			return ProcessActionUsage2(action);
		case BattleActionState_Animation:
			return ProcessActionAnimation(action);
		case BattleActionState_Execute:
			return ProcessActionExecute(action);
		case BattleActionState_Critical:
			return ProcessActionCritical(action);
		case BattleActionState_Apply:
			return ProcessActionApply(action);
		case BattleActionState_Failure:
			return ProcessActionFailure(action);
		case BattleActionState_Damage:
			return ProcessActionDamage(action);
		case BattleActionState_Params:
			return ProcessActionParamEffects(action);
		case BattleActionState_States:
			return ProcessActionStateEffects(action);
		case BattleActionState_Attributes:
			return ProcessActionAttributeEffects(action);
		case BattleActionState_Finished:
			return ProcessActionFinished(action);
	}

	assert(false && "Invalid BattleActionState!");

	return true;
}

bool Scene_Battle_Rpg2k::ProcessActionBegin(Game_BattleAlgorithm::AlgorithmBase* action) {
	enum SubState {
		eBegin = 0,
		eShowMessage,
		ePost,
	};

	auto* src = action->GetSource();

	if (battle_action_substate == eBegin) {
		assert(src->Exists());
		battle_message_window->Clear();

		bool show_message = false;
		src->NextBattleTurn();

		std::vector<int16_t> states_to_heal = src->BattleStateHeal();
		src->ApplyConditions();

		const lcf::rpg::State* pri_state = nullptr;
		bool pri_was_healed = false;
		for (size_t id = 1; id <= lcf::Data::states.size(); ++id) {
			auto was_healed = std::find(states_to_heal.begin(), states_to_heal.end(), id) != states_to_heal.end();
			if (!was_healed && !src->HasState(id)) {
				continue;
			}

			auto* state = lcf::ReaderUtil::GetElement(lcf::Data::states, id);
			if (!pri_state || state->priority >= pri_state->priority) {
				pri_state = state;
				pri_was_healed = was_healed;
			}
		}

		if (pri_state != nullptr) {
			StringView msg = pri_was_healed
				? pri_state->message_recovery
				: pri_state->message_affected;

			// RPG_RT behavior:
			// If state was healed, always prints.
			// If state is inflicted, only prints if msg not empty.
			if (pri_was_healed || !msg.empty()) {
				show_message = true;
				pending_message = ToString(msg);
			}
		}

		if (action->GetType() != Game_BattleAlgorithm::Type::Null || show_message) {
			SelectionFlash(action->GetSource());
		}

		if (show_message) {
			SetWait(4,4);
			return ProcessNextSubState(eShowMessage, action);
		}
		battle_action_substate = ePost;
	}

	if (battle_action_substate == eShowMessage) {
		battle_message_window->PushWithSubject(std::move(pending_message), action->GetSource()->GetName());
		SetWait(20, 60);
		pending_message.clear();
		return ProcessNextSubState(ePost, action);
	}

	if (battle_action_substate == ePost) {
		battle_message_window->Clear();

		if (action->GetType() == Game_BattleAlgorithm::Type::Null) {
			return true;
		}

		SetWait(4,4);
	}

	return ProcessNextAction(BattleActionState_Usage1, action);
}


bool Scene_Battle_Rpg2k::ProcessActionUsage1(Game_BattleAlgorithm::AlgorithmBase* action) {

	action->InitTargets();
	if (!action->IsCurrentTargetValid()) {
		return true;
	}

	battle_message_window->Clear();

	if (action->HasStartMessage()) {
		battle_message_window->Push(action->GetStartMessage());
		battle_message_window->ScrollToEnd();

		if (action->HasSecondStartMessage()) {
			SetWaitForUsage(action->GetType(), 0);
		}
	}

	return ProcessNextAction(BattleActionState_Usage2, action);
}

bool Scene_Battle_Rpg2k::ProcessActionUsage2(Game_BattleAlgorithm::AlgorithmBase* action) {
	if (action->HasSecondStartMessage()) {
		battle_message_window->Push(action->GetSecondStartMessage());
		battle_message_window->ScrollToEnd();
	}
	battle_action_start_index = battle_message_window->GetLineCount();

	auto* se = action->GetStartSe();
	if (se) {
		Main_Data::game_system->SePlay(*se);
	}

	return ProcessNextAction(BattleActionState_Animation, action);
}

bool Scene_Battle_Rpg2k::ProcessActionAnimation(Game_BattleAlgorithm::AlgorithmBase* action) {
	int frames = 0;
	while(1) {
		const int cur_anim = action->GetAnimationId(battle_action_substate_index);
		++battle_action_substate_index;
		int next_anim = 0;

		if (cur_anim) {
			if (action->GetTarget()->GetType() == Game_Battler::Type_Enemy) {
				frames = action->PlayAnimation(cur_anim);
			} else {
				frames = action->PlayAnimation(cur_anim, true, 40);
			}
			next_anim = action->GetAnimationId(battle_action_substate_index);
		}

		if (!next_anim) {
			break;
		}

		if (frames) {
			SetWait(frames, frames);
			return false;
		}
	}

	// Wait for last start message and last animation.
	SetWaitForUsage(action->GetType(), frames);

	return ProcessNextAction(BattleActionState_Execute, action);
}

bool Scene_Battle_Rpg2k::ProcessActionExecute(Game_BattleAlgorithm::AlgorithmBase* action) {
	action->Execute();
	if (action->GetType() == Game_BattleAlgorithm::Type::Normal
			|| action->GetType() == Game_BattleAlgorithm::Type::SelfDestruct) {
		SetWait(4,4);
		if (action->IsSuccess() && action->IsCriticalHit()) {
			return ProcessNextAction(BattleActionState_Critical, action);
		}
	}
	return ProcessNextAction(BattleActionState_Apply, action);
}

bool Scene_Battle_Rpg2k::ProcessActionCritical(Game_BattleAlgorithm::AlgorithmBase* action) {
	battle_message_window->Push(action->GetCriticalHitMessage());
	battle_message_window->ScrollToEnd();
	SetWait(10, 30);

	return ProcessNextAction(BattleActionState_Apply, action);
}

bool Scene_Battle_Rpg2k::ProcessActionApply(Game_BattleAlgorithm::AlgorithmBase* action) {
	action->ApplyFirstTimeEffect();

	battle_action_results_index = battle_message_window->GetLineCount();

	if (!action->IsSuccess()) {
		return ProcessNextAction(BattleActionState_Failure, action);
	}

	auto* target = action->GetTarget();

	if (!target) {
		return ProcessNextAction(BattleActionState_Finished, action);
	}

	return ProcessNextAction(BattleActionState_Damage, action);
}

bool Scene_Battle_Rpg2k::ProcessActionFailure(Game_BattleAlgorithm::AlgorithmBase* action) {
	enum SubState {
		eBegin = 0,
		eProcess,
	};

	if (battle_action_substate == eBegin) {
		SetWait(4,4);
		return ProcessNextSubState(eProcess, action);
	}

	auto* se = action->GetFailureSe();
	if (se) {
		Main_Data::game_system->SePlay(*se);
	}

	const auto& fail_msg = action->GetFailureMessage();
	battle_message_window->Push(fail_msg);
	battle_message_window->ScrollToEnd();
	SetWait(20, 60);

	return ProcessNextAction(BattleActionState_Finished, action);
}

bool Scene_Battle_Rpg2k::ProcessActionDamage(Game_BattleAlgorithm::AlgorithmBase* action) {
	enum SubState {
		eBegin = 0,
		eMessage,
		eApply,
		ePreStates,
		eStates,
		ePost,
	};

	if (battle_action_substate == eBegin) {
		if (!action->IsAffectHp() || action->GetAffectedHp() > 0 || ((action->IsPositive() || action->IsAbsorb()) && action->GetAffectedHp() == 0)) {
			return ProcessNextAction(BattleActionState_Params, action);
		}

		SetWait(4,4);
		return ProcessNextSubState(eMessage, action);
	}

	if (battle_action_substate == eMessage) {
		auto* target = action->GetTarget();
		assert(target);
		auto dmg = action->GetAffectedHp();

		if (!action->IsAbsorb()) {
			if (target->GetType() == Game_Battler::Type_Ally) {
				Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_AllyDamage));
				if (dmg < 0) {
					Main_Data::game_screen->ShakeOnce(3, 5, 8);
				}
			} else {
				Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_EnemyDamage));
			}
			if (target->GetType() == Game_Battler::Type_Enemy) {
				static_cast<Game_Enemy*>(target)->SetBlinkTimer();
			}
		}

		std::string msg;
		if (action->IsAbsorb()) {
			msg = action->GetHpSpAbsorbedMessage(-dmg, lcf::Data::terms.health_points);
		} else {
			if (dmg == 0) {
				msg = action->GetUndamagedMessage();
			} else {
				msg = action->GetDamagedMessage(-dmg);
			}
		}

		battle_message_window->Push(msg);
		battle_message_window->ScrollToEnd();
		if (action->IsAbsorb()) {
			SetWait(20, 60);
		} else {
			SetWait(20, 40);
		}

		return ProcessNextSubState(eApply, action);
	}

	if (battle_action_substate == eApply) {
		// Hp damage is delayed until after the message, so that the target death animation
		// occurs at the right time.
		action->ApplyHpEffect();

		auto* target = action->GetTarget();
		assert(target);
		if (target->IsDead()) {
			ProcessDeath(action);
		}

		battle_action_dmg_index = battle_message_window->GetLineCount();

		return ProcessNextSubState(ePreStates, action);
	}

	if (battle_action_substate == ePreStates) {
		const auto& states = action->GetStateEffects();
		auto& idx = battle_action_substate_index;
		for (;idx < static_cast<int>(states.size()); ++idx) {
			auto& se = states[idx];
			auto* state = lcf::ReaderUtil::GetElement(lcf::Data::states, se.state_id);
			if (!state || se.effect != Game_BattleAlgorithm::StateEffect::HealedByAttack) {
				continue;
			}
			action->ApplyStateEffect(se);
			pending_message = action->GetStateMessage(state->message_recovery);
			++battle_action_substate_index;

			battle_message_window->PopUntil(battle_action_dmg_index);
			battle_message_window->ScrollToEnd();
			SetWait(4,4);

			return ProcessNextSubState(eStates, action, false);
		}
		return ProcessNextSubState(ePost, action);
	}

	if (battle_action_substate == eStates) {
		battle_message_window->Push(pending_message);
		battle_message_window->ScrollToEnd();
		SetWait(20, 40);

		return ProcessNextSubState(ePreStates, action, false);
	}

	if (battle_action_substate == ePost) {
		SetWait(0, 10);
	}

	return ProcessNextAction(BattleActionState_Params, action);
}

bool Scene_Battle_Rpg2k::ProcessActionParamEffects(Game_BattleAlgorithm::AlgorithmBase* action) {
	enum SubState {
		ePreHp,
		eHp,
		ePreSp,
		eSp,
		ePreAtk,
		eAtk,
		ePreDef,
		eDef,
		ePreSpi,
		eSpi,
		ePreAgi,
		eAgi,
		eDone
	};

	const auto next_state = BattleActionState_States;

	// All of the "Pre" states are even numbers, so catch all Pre here.
	if ((battle_action_substate & 1) == 0) {
		pending_message.clear();

		auto checkNext = [&]() {
			if (pending_message.empty()) {
				SetBattleActionSubState(battle_action_substate + 2);
			}
		};

		if (battle_action_substate == ePreHp) {
			// Damage is handled by Damage state, so only check healing here.
			if (action->GetAffectedHp() > 0 && !action->IsRevived()) {
				auto hp = action->ApplyHpEffect();
				pending_message = action->GetHpSpRecoveredMessage(hp, lcf::Data::terms.health_points);
			}
			checkNext();
		}

		if (battle_action_substate == ePreSp) {
			auto sp = action->ApplySpEffect();
			if (action->IsAbsorb()) {
				pending_message = action->GetHpSpAbsorbedMessage(-sp, lcf::Data::terms.spirit_points);
			} else {
				if (sp > 0) {
					pending_message = action->GetHpSpRecoveredMessage(sp, lcf::Data::terms.spirit_points);
				}
				if (sp < 0) {
					pending_message = action->GetParameterChangeMessage(sp, lcf::Data::terms.spirit_points);
				}
			}
			checkNext();
		}

		if (battle_action_substate == ePreAtk) {
			auto atk = action->ApplyAtkEffect();
			if (atk != 0) {
				pending_message = action->GetParameterChangeMessage(atk, lcf::Data::terms.attack);
			}
			checkNext();
		}

		if (battle_action_substate == ePreDef) {
			auto def = action->ApplyDefEffect();
			if (def != 0) {
				pending_message = action->GetParameterChangeMessage(def, lcf::Data::terms.defense);
			}
			checkNext();
		}

		if (battle_action_substate == ePreSpi) {
			auto spi = action->ApplySpiEffect();
			if (spi != 0) {
				pending_message = action->GetParameterChangeMessage(spi, lcf::Data::terms.spirit);
			}
			checkNext();
		}

		if (battle_action_substate == ePreAgi) {
			auto agi = action->ApplyAgiEffect();
			if (agi != 0) {
				pending_message = action->GetParameterChangeMessage(agi, lcf::Data::terms.agility);
			}
			checkNext();
		}

		if (!pending_message.empty()) {
			battle_message_window->PopUntil(battle_action_results_index);
			battle_message_window->ScrollToEnd();
			SetWait(4,4);

			return ProcessNextSubState(battle_action_substate + 1, action, false);
		}
	}

	// Use >= here so that the each "pre" stage above can just call
	// checkNext() to increment +2 without worrying about overflowing
	// past eDone.
	if (battle_action_substate >= eDone) {
		return ProcessNextAction(next_state, action);
	}

	// All of the normal states are odd numbers.
	if ((battle_action_substate & 1) != 0) {
		battle_message_window->Push(pending_message);
		battle_message_window->ScrollToEnd();
		SetWait(20, 60);

		return ProcessNextSubState(battle_action_substate + 1, action);
	}

	return ProcessNextAction(next_state, action);
}

bool Scene_Battle_Rpg2k::ProcessActionStateEffects(Game_BattleAlgorithm::AlgorithmBase* action) {
	enum SubState {
		eApply,
		ePreWait,
		eMessage,
		eDone
	};

	const auto next_state = BattleActionState_Attributes;

	auto* target = action->GetTarget();

	if (battle_action_substate == eApply) {
		pending_message.clear();

		const auto was_dead = target->IsDead();
		const auto& states = action->GetStateEffects();
		auto& idx = battle_action_substate_index;

		if (idx >= static_cast<int>(states.size())) {
			return ProcessNextAction(next_state, action);
		}

		for (;idx < (int)states.size(); ++idx) {
			auto& se = states[idx];
			auto* state = lcf::ReaderUtil::GetElement(lcf::Data::states, se.state_id);
			if (!state) {
				continue;
			}

			action->ApplyStateEffect(se);
			bool is_actor = target->GetType() == Game_Battler::Type_Ally;
			switch (se.effect) {
				case Game_BattleAlgorithm::StateEffect::Inflicted:
					pending_message = action->GetStateMessage(is_actor ? state->message_actor : state->message_enemy);
					break;
				case Game_BattleAlgorithm::StateEffect::Healed:
					pending_message = action->GetStateMessage(state->message_recovery);
					break;
				case Game_BattleAlgorithm::StateEffect::AlreadyInflicted:
					pending_message = action->GetStateMessage(state->message_already);
					break;
				default:
					break;
			}

			if ((!was_dead && target->IsDead()) || !pending_message.empty()) {
				break;
			}
		}

		battle_message_window->PopUntil(battle_action_results_index);
		battle_message_window->ScrollToEnd();

		// If we were killed by state
		if (!was_dead && target->IsDead()) {
			ProcessDeath(action);
		}
		return ProcessNextSubState(ePreWait, action, false);
	}

	if (battle_action_substate == ePreWait) {
		SetWait(4,4);
		return ProcessNextSubState(eMessage, action, false);
	}

	if (battle_action_substate == eMessage) {
		battle_message_window->Push(pending_message);
		battle_message_window->ScrollToEnd();
		SetWait(20, 60);

		// Process the next state
		++battle_action_substate_index;
		return ProcessNextSubState(eApply, action, false);
	}

	return ProcessNextAction(next_state, action);
}

bool Scene_Battle_Rpg2k::ProcessActionAttributeEffects(Game_BattleAlgorithm::AlgorithmBase* action) {
	enum SubState {
		eApply,
		eMessage,
	};

	const auto next_state = BattleActionState_Finished;

	// All of the "Pre" states are even numbers, so catch all Pre here.
	if (battle_action_substate == eApply) {
		pending_message.clear();

		const auto& attrs = action->GetShiftedAttributes();
		auto& idx = battle_action_substate_index;
		if (idx >= static_cast<int>(attrs.size())) {
			return ProcessNextAction(next_state, action);
		}

		for (;idx < (int)attrs.size(); ++idx) {
			auto& ae = attrs[battle_action_substate_index];
			auto shifted = action->ApplyAttributeShiftEffect(ae);
			if (shifted != 0) {
				pending_message = action->GetAttributeShiftMessage(shifted, lcf::ReaderUtil::GetElement(lcf::Data::attributes, ae.attr_id)->name);
				break;
			}
		}

		battle_message_window->PopUntil(battle_action_results_index);
		battle_message_window->ScrollToEnd();
		SetWait(4,4);

		return ProcessNextSubState(eMessage, action, false);
	}

	// All of the normal states are odd numbers.
	if (battle_action_substate == eMessage) {
		battle_message_window->Push(pending_message);
		battle_message_window->ScrollToEnd();
		SetWait(20, 60);

		++battle_action_substate_index;
		return ProcessNextSubState(eApply, action, false);
	}

	return ProcessNextAction(next_state, action);
}

void Scene_Battle_Rpg2k::ProcessDeath(Game_BattleAlgorithm::AlgorithmBase* action) {
	auto* target = action->GetTarget();
	assert(target);

	battle_message_window->Push(action->GetDeathMessage());
	battle_message_window->ScrollToEnd();
	SetWait(36, 60);

	if (target->GetType() == Game_Battler::Type_Enemy) {
		static_cast<Game_Enemy*>(target)->SetDeathTimer();
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_EnemyKill));
	}
}

bool Scene_Battle_Rpg2k::ProcessActionFinished(Game_BattleAlgorithm::AlgorithmBase* action) {
	if (action->RepeatNext(true) || action->TargetNext()) {
		// Clear the console for the next target
		battle_message_window->PopUntil(battle_action_start_index);
		battle_message_window->ScrollToEnd();

		return ProcessNextAction(BattleActionState_Execute, action);
	}

	battle_message_window->Clear();
	return true;
}

void Scene_Battle_Rpg2k::ProcessInput() {
	if (IsWindowMoving()) {
		return;
	}
	if (Input::IsTriggered(Input::DECISION)) {
		switch (state) {
		case State_Start:
			// no-op
			break;
		case State_SelectOption:
			// Interpreter message boxes pop up in this state
			if (!message_window->IsVisible()) {
				OptionSelected();
			}
			break;
		case State_SelectActor:
			SetState(State_SelectCommand);
			SelectNextActor();
			break;
		case State_AutoBattle:
			// no-op
			break;
		case State_SelectCommand:
			CommandSelected();
			break;
		case State_SelectEnemyTarget:
			EnemySelected();
			break;
		case State_SelectAllyTarget:
			AllySelected();
			break;
		case State_Battle:
			// no-op
			break;
		case State_SelectItem:
			ItemSelected();
			break;
		case State_SelectSkill:
			SkillSelected();
			break;
		case State_Victory:
		case State_Defeat:
		case State_Escape:
			// no-op
			break;
		}
	}

	if (Input::IsTriggered(Input::CANCEL)) {
		switch (state) {
		case State_Start:
		case State_SelectOption:
			// no-op
			break;
		case State_SelectActor:
		case State_AutoBattle:
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cancel));
			SetState(State_SelectOption);
			break;
		case State_SelectCommand:
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cancel));
			--actor_index;
			SelectPreviousActor();
			break;
		case State_SelectItem:
		case State_SelectSkill:
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cancel));
			SetState(State_SelectCommand);
			break;
		case State_SelectEnemyTarget:
		case State_SelectAllyTarget:
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cancel));
			SetState(previous_state);
			break;
		case State_Battle:
			// no-op
			break;
		case State_Victory:
		case State_Defeat:
		case State_Escape:
			// no-op
			break;
		}
	}
	if (Input::IsTriggered(Input::DEBUG_MENU)) {
		this->CallDebug();
	}
}

void Scene_Battle_Rpg2k::OptionSelected() {
	switch (options_window->GetIndex()) {
		case 0: // Battle
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Decision));
			CreateBattleTargetWindow();
			auto_battle = false;
			SetState(State_SelectActor);
			break;
		case 1: // Auto Battle
			auto_battle = true;
			SetState(State_AutoBattle);
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Decision));
			break;
		case 2: // Escape
			if (!IsEscapeAllowed()) {
				Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Buzzer));
			}
			else {
				Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Decision));
				SetState(State_Escape);
			}
			break;
	}
}

void Scene_Battle_Rpg2k::CommandSelected() {

	switch (command_window->GetIndex()) {
		case 0: // Attack
			AttackSelected();
			break;
		case 1: // Skill
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Decision));
			SetState(State_SelectSkill);
			break;
		case 2: // Defense
			DefendSelected();
			break;
		case 3: // Item
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Decision));
			SetState(State_SelectItem);
			break;
		default:
			// no-op
			break;
	}
}

void Scene_Battle_Rpg2k::Escape() {
	enum SubState {
		eBegin = 0,
		eSuccess = 1,
		eFailure = 2,
	};

	if (!CheckWait()) {
		return;
	}

	if (battle_action_substate == eBegin) {
		battle_message_window->Clear();

		auto next_ss = TryEscape() ? eSuccess : eFailure;

		if (next_ss == eSuccess) {
			battle_message_window->Push(lcf::Data::terms.escape_success);
		} else {
			battle_message_window->Push(lcf::Data::terms.escape_failure);
		}
		SetWait(10, 60);
		SetBattleActionSubState(next_ss);
		// To count this frame in CheckWait() we recurse just like in ProcessBattleActions.
		Escape();
		return;
	}

	if (battle_action_substate == eSuccess) {
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Escape));

		EndBattle(BattleResult::Escape);
		return;
	}

	if (battle_action_substate == eFailure) {
		SetState(State_Battle);
		NextTurn();

		CreateEnemyActions();
		CreateExecutionOrder();
		Game_Battle::RefreshEvents();
		return;
	}
}

void Scene_Battle_Rpg2k::SelectNextActor() {
	std::vector<Game_Actor*> allies = Main_Data::game_party->GetActors();

	if ((size_t)actor_index == allies.size()) {
		// All actor actions decided, player turn ends
		SetState(State_Battle);
		NextTurn();

		CreateEnemyActions();
		CreateExecutionOrder();
		Game_Battle::RefreshEvents();

		return;
	}

	active_actor = allies[actor_index];
	status_window->SetIndex(actor_index);
	actor_index++;

	Game_Battler* random_target = NULL;

	if (!active_actor->CanAct()) {
		active_actor->SetBattleAlgorithm(std::make_shared<Game_BattleAlgorithm::NoMove>(active_actor));
		battle_actions.push_back(active_actor);
		SelectNextActor();
		return;
	}

	switch (active_actor->GetSignificantRestriction()) {
		case lcf::rpg::State::Restriction_attack_ally:
			random_target = Main_Data::game_party->GetRandomActiveBattler();
			break;
		case lcf::rpg::State::Restriction_attack_enemy:
			random_target = Main_Data::game_enemyparty->GetRandomActiveBattler();
			break;
		default:
			break;
	}

	if (random_target) {
		// RPG_RT doesn't support "Attack All" weapons when battler is confused or provoked.
		active_actor->SetBattleAlgorithm(std::make_shared<Game_BattleAlgorithm::Normal>(active_actor, random_target));
		battle_actions.push_back(active_actor);

		SelectNextActor();
		return;
	}

	if (auto_battle || active_actor->GetAutoBattle()) {
		this->autobattle_algo->SetAutoBattleAction(*active_actor);
		assert(active_actor->GetBattleAlgorithm() != nullptr);
		battle_actions.push_back(active_actor);

		SelectNextActor();
		return;
	}

	SetState(Scene_Battle::State_SelectCommand);
}

void Scene_Battle_Rpg2k::SelectPreviousActor() {
	std::vector<Game_Actor*> allies = Main_Data::game_party->GetActors();

	if (allies[0] == active_actor) {
		SetState(State_SelectOption);
		actor_index = 0;
		return;
	}

	actor_index--;
	active_actor = allies[actor_index];

	battle_actions.back()->SetBattleAlgorithm(std::shared_ptr<Game_BattleAlgorithm::AlgorithmBase>());
	battle_actions.pop_back();

	if (!active_actor->IsControllable()) {
		SelectPreviousActor();
		return;
	}

	SetState(State_SelectActor);
}

void Scene_Battle_Rpg2k::CreateExecutionOrder() {
	// Define random Agility. Must be done outside of the sort function because of the "strict weak ordering" property, so the sort is consistent
	for (auto battler : battle_actions) {
		int battle_order = battler->GetAgi() + Rand::GetRandomNumber(0, battler->GetAgi() / 4 + 3);
		if (battler->GetBattleAlgorithm()->GetType() == Game_BattleAlgorithm::Type::Normal && battler->HasPreemptiveAttack()) {
			// RPG_RT sets this value
			battle_order += 9999;
		}
		battler->SetBattleOrderAgi(battle_order);
	}
	std::sort(battle_actions.begin(), battle_actions.end(),
			[](Game_Battler* l, Game_Battler* r) {
			return l->GetBattleOrderAgi() > r->GetBattleOrderAgi();
			});

	for (const auto& battler : battle_actions) {
		if (std::count(battle_actions.begin(), battle_actions.end(), battler) > 1) {
			Output::Warning("CreateExecutionOrder: Battler {} ({}) has multiple battle actions", battler->GetId(), battler->GetName());
			Output::Warning("Please report a bug!");
			break;
		}
	}
}

void Scene_Battle_Rpg2k::CreateEnemyActions() {
	if (first_strike) {
		return;
	}

	for (auto* enemy : Main_Data::game_enemyparty->GetEnemies()) {
		if (!EnemyAi::SetStateRestrictedAction(*enemy)) {
			enemyai_algo->SetEnemyAiAction(*enemy);
		}
		assert(enemy->GetBattleAlgorithm() != nullptr);
		ActionSelectedCallback(enemy);
	}
}

void Scene_Battle_Rpg2k::SetWait(int min_wait, int max_wait) {
#ifdef EP_DEBUG_BATTLE2K_MESSAGE
	Output::Debug("Wait({},{})", min_wait, max_wait);
#endif
	battle_action_wait = max_wait;
	battle_action_min_wait = max_wait - min_wait;
}

void Scene_Battle_Rpg2k::SetWaitForUsage(Game_BattleAlgorithm::Type type, int anim_frames) {
	int min_wait = 0;
	int max_wait = 0;
	switch (type) {
		case Game_BattleAlgorithm::Type::Normal:
			min_wait = 20;
			max_wait = 40;
			break;
		case Game_BattleAlgorithm::Type::Escape:
			min_wait = 36;
			max_wait = 60;
			break;
		case Game_BattleAlgorithm::Type::NoMove:
			min_wait = max_wait = 0;
			break;
		default:
			min_wait = 20;
			max_wait = 60;
			break;
	}
	SetWait(std::max(min_wait, anim_frames), std::max(max_wait, anim_frames));
}

bool Scene_Battle_Rpg2k::CheckWait() {
	if (battle_action_wait > 0) {
		if (Input::IsPressed(Input::CANCEL)) {
			return false;
		}
		--battle_action_wait;
		if (battle_action_wait > battle_action_min_wait) {
			return false;
		}
		if (!Input::IsPressed(Input::DECISION)
			&& !Input::IsPressed(Input::SHIFT)
			&& battle_action_wait > 0) {
			return false;
		}
		battle_action_wait = 0;
	}
	return true;
}


bool Scene_Battle_Rpg2k::DisplayMonstersInMessageWindow() {
	if (encounter_message_first_monster) {
		std::vector<Game_Battler *> visible_enemies;
		// First time entered, initialize.
		Main_Data::game_enemyparty->GetActiveBattlers(visible_enemies);

		for (auto& enemy: visible_enemies) {
			// Format and wordwrap all messages, then pull them out and push them back 1 at a time.
			battle_message_window->PushWithSubject(lcf::Data::terms.encounter, enemy->GetName());
		}

		battle_result_messages = battle_message_window->GetLines();
		battle_result_messages_it = battle_result_messages.begin();
		battle_message_window->Clear();

		encounter_message_first_strike = false;
		encounter_message_first_monster = false;

		SetWait(4, 4);
		return DisplayMonstersInMessageWindow();
	}

	if (!CheckWait()) {
		return false;
	}

	if (battle_result_messages_it == battle_result_messages.end()) {
		battle_message_window->Clear();
		if (first_strike && !encounter_message_first_strike) {
			battle_message_window->Push(lcf::Data::terms.special_combat);
			encounter_message_first_strike = true;
			SetWait(30, 70);
			return DisplayMonstersInMessageWindow();;
		}
		else {
			// reset static vars
			encounter_message_first_strike = false;
			encounter_message_first_monster = true;
			battle_result_messages.clear();
			battle_result_messages_it = battle_result_messages.end();
			return true;
		}
	}

	if (battle_message_window->IsPageFilled()) {
		battle_message_window->Clear();
	}

	battle_message_window->Push(*battle_result_messages_it);
	++battle_result_messages_it;

	if (battle_result_messages_it == battle_result_messages.end() ||
			battle_message_window->IsPageFilled()) {
		SetWait(30, 70);
	}
	else {
		SetWait(8, 8);
	}

	return DisplayMonstersInMessageWindow();
}

void Scene_Battle_Rpg2k::PushExperienceGainedMessage(PendingMessage& pm, int exp) {
	if (Player::IsRPG2kE()) {
		pm.PushLine(
			Utils::ReplacePlaceholders(
				lcf::Data::terms.exp_received,
				Utils::MakeArray('V', 'U'),
				Utils::MakeSvArray(std::to_string(exp), lcf::Data::terms.exp_short)
			) + Player::escape_symbol + "."
		);
	}
	else {
		std::stringstream ss;
		ss << exp << lcf::Data::terms.exp_received << Player::escape_symbol << ".";
		pm.PushLine(ss.str());
	}
}

void Scene_Battle_Rpg2k::PushGoldReceivedMessage(PendingMessage& pm, int money) {

	if (Player::IsRPG2kE()) {
		pm.PushLine(
			Utils::ReplacePlaceholders(
				lcf::Data::terms.gold_recieved_a,
				Utils::MakeArray('V', 'U'),
				Utils::MakeSvArray(std::to_string(money), lcf::Data::terms.gold)
			) + Player::escape_symbol + "."
		);
	}
	else {
		std::stringstream ss;
		ss << lcf::Data::terms.gold_recieved_a << " " << money << lcf::Data::terms.gold << lcf::Data::terms.gold_recieved_b << Player::escape_symbol << ".";
		pm.PushLine(ss.str());
	}
}

void Scene_Battle_Rpg2k::PushItemRecievedMessages(PendingMessage& pm, std::vector<int> drops) {
	std::stringstream ss;

	for (std::vector<int>::iterator it = drops.begin(); it != drops.end(); ++it) {
		const lcf::rpg::Item* item = lcf::ReaderUtil::GetElement(lcf::Data::items, *it);
		// No Output::Warning needed here, reported later when the item is added
		StringView item_name = item ? StringView(item->name) : StringView("??? BAD ITEM ???");

		if (Player::IsRPG2kE()) {
			pm.PushLine(
				Utils::ReplacePlaceholders(
					lcf::Data::terms.item_recieved,
					Utils::MakeArray('S'),
					Utils::MakeSvArray(item_name)
				) + Player::escape_symbol + "."
			);
		}
		else {
			ss.str("");
			ss << item_name << lcf::Data::terms.item_recieved << Player::escape_symbol << ".";
			pm.PushLine(ss.str());
		}
	}
}

bool Scene_Battle_Rpg2k::CheckWin() {
	if (Game_Battle::CheckWin()) {
		SetState(State_Victory);

		int exp = Main_Data::game_enemyparty->GetExp();
		int money = Main_Data::game_enemyparty->GetMoney();
		std::vector<int> drops;
		Main_Data::game_enemyparty->GenerateDrops(drops);

		auto pm = PendingMessage();
		pm.SetEnableFace(false);

		pm.SetWordWrapped(Player::IsRPG2kE());
		pm.PushLine(ToString(lcf::Data::terms.victory) + Player::escape_symbol + "|");

		std::stringstream ss;
		if (exp > 0) {
			PushExperienceGainedMessage(pm, exp);
		}
		if (money > 0) {
			PushGoldReceivedMessage(pm, money);
		}
		PushItemRecievedMessages(pm, drops);

		Main_Data::game_system->BgmPlay(Main_Data::game_system->GetSystemBGM(Main_Data::game_system->BGM_Victory));

		// Update attributes
		std::vector<Game_Battler*> ally_battlers;
		Main_Data::game_party->GetActiveBattlers(ally_battlers);

		pm.PushPageEnd();

		for (auto& ally: ally_battlers) {
			Game_Actor* actor = static_cast<Game_Actor*>(ally);
			actor->ChangeExp(actor->GetExp() + exp, &pm);
		}
		Main_Data::game_party->GainGold(money);
		for (auto& item: drops) {
			Main_Data::game_party->AddItem(item, 1);
		}

		Game_Message::SetPendingMessage(std::move(pm));

		return true;
	}

	return false;
}

bool Scene_Battle_Rpg2k::CheckLose() {
	if (Game_Battle::CheckLose()) {
		SetState(State_Defeat);

		Main_Data::game_system->SetMessagePositionFixed(true);
		Main_Data::game_system->SetMessagePosition(2);
		Main_Data::game_system->SetMessageTransparent(false);

		auto pm = PendingMessage();
		pm.SetEnableFace(false);

		pm.SetWordWrapped(Player::IsRPG2kE());

		pm.PushLine(ToString(lcf::Data::terms.defeat));

		Main_Data::game_system->BgmPlay(Main_Data::game_system->GetSystemBGM(Main_Data::game_system->BGM_GameOver));

		Game_Message::SetPendingMessage(std::move(pm));

		return true;
	}

	return false;
}

bool Scene_Battle_Rpg2k::CheckResultConditions() {
	return CheckLose() || CheckWin();
}

