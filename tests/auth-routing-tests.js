// Exercise production QML routing functions with a deterministic auth peer.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const path = require('path');
const assets = path.join(__dirname, '..', 'assets');
function method(file, name) {
    const source = fs.readFileSync(path.join(assets, file), 'utf8');
    const match = source.match(new RegExp('function ' + name + '\\(\\)\\s*\\{[\\s\\S]*?\\n    }'));
    assert(match, name + ' must remain inspectable');
    return match[0];
}
let opens = 0, pops = 0, saved = 0;
const context = {
    authPhase: 'parameters', autoConfigAttempted: false, loginStarted: false,
    _telegram: {hasSavedConfiguration: true, busy: false, useSavedConfiguration() {saved++; return true;}},
    openLogin() {opens++;},
    navigation: {top: {objectName: 'chatsPage'}, pop() {pops++;}}
};
vm.createContext(context);
vm.runInContext(method('main.qml', 'applyAuthorization'), context);
context.applyAuthorization();
assert.equal(saved, 1); assert.equal(opens, 0);
context.applyAuthorization(); assert.equal(saved, 1);
context.authPhase = 'ready'; context.navigation.top.objectName = 'loginPage';
context.applyAuthorization(); assert.equal(pops, 1);
context.authPhase = 'starting'; context.applyAuthorization();
context.authPhase = 'parameters'; context._telegram.hasSavedConfiguration = false;
context.applyAuthorization(); assert.equal(opens, 0);
context.loginStarted = true; context.applyAuthorization(); assert.equal(opens, 1);
context.authPhase = 'password'; context.applyAuthorization(); assert.equal(opens, 2);
context._telegram.hasSavedConfiguration = true; context._telegram.busy = true;
context.authPhase = 'parameters'; context.autoConfigAttempted = false;
context.applyAuthorization(); assert.equal(saved, 1);
const removed = [];
context.chatsPage = {objectName: 'chatsPage'};
const stack = [context.chatsPage, {objectName: 'settingsPage'}, {objectName: 'diagnosticsPage'}];
Object.defineProperty(context.navigation, 'top', {get: () => stack[stack.length - 1]});
context.navigation.remove = page => stack.pop() === page;
context.releasePage = page => removed.push(page.objectName);
context.setArchive = value => {context.archiveView = value;};
context.openLogin = () => stack.push({objectName: 'loginPage'});
context.demo = context.archiveView = true;
context.authPhase = 'loggingOut';
context.applyAuthorization();
assert.deepEqual(removed, ['diagnosticsPage', 'settingsPage']);
assert.deepEqual(stack.map(page => page.objectName), ['chatsPage', 'loginPage']);
assert.equal(context.demo, false);
assert.equal(context.archiveView, false);

const mainQml = fs.readFileSync(path.join(assets, 'main.qml'), 'utf8');
const logOutAction = mainQml.match(/objectName: "logOutAction"\s+title: ([^\n]+)\s+enabled: ([^\n]+)\s+onTriggered: \{([^}]+)\}/);
assert(logOutAction, 'The chat menu must expose the logout action');
let dialogs = 0, logOuts = 0;
const menu = {
    live: true, demo: false, qsTr: text => text,
    _telegram: {busy: false, logOut() {logOuts++;}},
    logOutDialog: {show() {dialogs++;}},
    System: {SystemUiResult: {ConfirmButtonSelection: 2}}
};
menu.navigation = menu;
vm.createContext(menu);
vm.runInContext(method('main.qml', 'requestLogOut'), menu);
assert.equal(vm.runInContext(logOutAction[1], menu), 'Выйти из аккаунта');
assert.equal(vm.runInContext(logOutAction[2], menu), true);
vm.runInContext(logOutAction[3], menu);
assert.equal(dialogs, 1);
assert.equal(logOuts, 0);
const confirm = mainQml.match(/onFinished: (if \(value === System\.SystemUiResult\.ConfirmButtonSelection\) _telegram\.logOut\(\))/);
assert(confirm, 'The shared dialog must require confirmation');
menu.value = 3;
vm.runInContext(confirm[1], menu);
assert.equal(logOuts, 0);
menu.value = 2;
vm.runInContext(confirm[1], menu);
assert.equal(logOuts, 1);
menu._telegram.busy = true;
assert.equal(vm.runInContext(logOutAction[2], menu), false);
menu.requestLogOut();
assert.equal(dialogs, 1);
menu.live = false;
menu.demo = true;
assert.equal(vm.runInContext(logOutAction[1], menu), 'Выйти из демо');
vm.runInContext(logOutAction[3], menu);
assert.equal(menu.demo, false);
assert.equal(logOuts, 1);
console.log('Chat menu logout passed: confirmation, cancellation, busy guard and demo exit.');

let configured = 0;
const preview = {loginActive: false, previewing: true, phase: 'parameters', _telegram: {
    busy: false, hasSavedConfiguration: false, testConfigurationAvailable: true,
    useTestConfiguration() {configured++;}
}};
vm.createContext(preview);
vm.runInContext(method('LoginPage.qml', 'chooseConnection'), preview);
preview.chooseConnection(); assert.equal(configured, 0);
preview.previewing = false; preview.chooseConnection(); assert.equal(configured, 0);
preview.loginActive = true; preview.chooseConnection(); assert.equal(configured, 1);
console.log('Auth routing passed: saved entry, ready transition, fresh entry, busy guard and inert previews.');

const loadingExpression = fs.readFileSync(path.join(assets, 'main.qml'), 'utf8').match(/property bool restoringSession: ([^\n]+)/)[1];
const startup = {previewWelcome: false, live: false, authPhase: 'parameters', _telegram: {available: true, hasSavedConfiguration: true, errorText: ''}};
vm.createContext(startup);
assert.equal(vm.runInContext(loadingExpression, startup), true);
startup._telegram.errorText = 'The database could not be opened';
assert.equal(vm.runInContext(loadingExpression, startup), false, 'A rejected restore must never leave the opening cover spinning');
startup._telegram.errorText = ''; startup._telegram.available = false;
assert.equal(vm.runInContext(loadingExpression, startup), false, 'A missing engine must not display an endless opening cover');
console.log('Startup cover passed: errors and unavailable engines leave loading state.');
