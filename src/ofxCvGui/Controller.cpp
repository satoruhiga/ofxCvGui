#include "ofxCvGui/Controller.h"
#include "ofxCvGui/InspectController.h"
#include "ofxAssets.h"

namespace ofxCvGui {
	//----------
	Controller::Controller() {
		this->initialised = false;
		this->maximised = false;
		this->chromeVisible = true;
		this->mouseOwner = nullptr;
		this->lastClickOwner = nullptr;
		this->lastMouseClick = pair<long long, ofMouseEventArgs>(std::numeric_limits<long long>::min(), ofMouseEventArgs());
		this->cachedWidth = 0.0f;
		this->cachedHeight = 0.0f;
	}

	//----------
	void Controller::init(shared_ptr<Panels::Groups::Base> rootGroup) {
		ofBackground(30);

		ofAddListener(ofEvents().update, this, &Controller::update);
		ofAddListener(ofEvents().draw, this, &Controller::draw);
		ofAddListener(ofEvents().mouseMoved, this, &Controller::mouseMoved);
		ofAddListener(ofEvents().mousePressed, this, &Controller::mousePressed);
		ofAddListener(ofEvents().mouseReleased, this, &Controller::mouseReleased);
		ofAddListener(ofEvents().mouseDragged, this, &Controller::mouseDragged);
		ofAddListener(ofEvents().keyPressed, this, &Controller::keyPressed);	
		ofAddListener(ofEvents().fileDragEvent, this, &Controller::filesDragged);
		ofAddListener(ofEvents().windowResized, this, &Controller::windowResized);

		ofxAssets::Register::X().addAddon("ofxCvGui");
		
		rootGroup->setBounds(ofGetCurrentViewport());
		this->rootGroup = rootGroup;
		this->currentPanel = PanelPtr();
		this->currentPanelBounds = ofGetCurrentViewport();

		//cache fonts
		ofxAssets::font("ofxCvGui::swisop3", 12);
		ofxAssets::font("ofxCvGui::swisop3", 14);
		ofxAssets::font("ofxCvGui::swisop3", 18);
		ofxAssets::font("ofxCvGui::swisop3", 24);

		this->initialised = true;
	}
	
	//----------
	void Controller::add(PanelPtr panel) {
		if (!initialised)
			return;
		this->rootGroup->add(panel);
	}

	//----------
	void Controller::drop(PanelPtr panel) {
		if (!initialised)
			return;
		this->rootGroup->drop(panel);
	}

	//----------
	void Controller::clear() {
		if (!initialised)
			return;
		this->rootGroup->clear();
	}
	
	//----------
	void Controller::toggleFullscreen() {
		ofToggleFullscreen();
	}

	//----------
	void Controller::toggleMaximised() {
		if (!this->maximised) {
			//maximise current panel
			auto currentPanel = this->currentPanel.lock();
			if (currentPanel) {
				this->setMaximised(currentPanel);
				currentPanel->setBounds(ofGetCurrentViewport());
			}
		} else {
			//clear maximise
			this->clearMaximised();
		}
	}
	
	//----------
	void Controller::setMaximised(PanelPtr panel) {
		this->maximised = true;
		this->currentPanel = panel;
		this->currentPanelBounds = ofGetCurrentViewport();
		panel->setBounds(ofRectangle(0, 0, ofGetScreenWidth(), ofGetScreenHeight()));
	}

	//----------
	void Controller::clearMaximised() {
		this->maximised = false;
		rootGroup->setBounds(ofGetCurrentViewport());
		this->updateCurrentPanel();
	}

	//----------
	void Controller::showChrome() {
		this->chromeVisible = true;
	}
	
	//----------
	void Controller::hideChrome() {
		this->chromeVisible = false;
	}
	
	//----------
	void Controller::update(ofEventArgs& args) {
		if (!initialised) {
			return;
		}
		InspectController::X().update();
		rootGroup->update();
	}

	//----------
	void Controller::draw(ofEventArgs& args) {
		if (!initialised) {
			return;
		}

		DrawArguments rootDrawArguments;
		rootDrawArguments.chromeEnabled = this->chromeVisible;
		rootDrawArguments.naturalBounds = ofGetCurrentViewport();
		rootDrawArguments.globalTransform = ofMatrix4x4();
		rootDrawArguments.globalScale = 1.0f;
		rootDrawArguments.localBounds = ofRectangle(0, 0, rootDrawArguments.naturalBounds.getWidth(), rootDrawArguments.naturalBounds.getHeight());
		rootDrawArguments.globalBounds = rootDrawArguments.naturalBounds;

		auto currentPanel = this->currentPanel.lock();

		if (this->maximised) {
            DrawArguments arg(rootDrawArguments);
			currentPanel->draw(arg);
		} else {
			//highlight panel
			if (currentPanel) {
				ofPushStyle();
				ofEnableAlphaBlending();
				ofSetColor(40, 40, 40, 100);
				ofDrawRectangle(this->currentPanelBounds);
				ofPopStyle();
			}

			this->rootGroup->draw(rootDrawArguments);
		}
	}

	//----------
	PanelGroupPtr Controller::getRootGroup() {
		return this->rootGroup;
	}

	//----------
	PanelPtr Controller::getPanelUnderCursor(const ofVec2f & position) {
		if (this->maximised) {
			return currentPanel.lock();
		} else {
			ofRectangle panelBounds = this->rootGroup->getBounds();
			return this->findPanelUnderCursor(panelBounds, position);
		}
	}

	//----------
	void Controller::mouseMoved(ofMouseEventArgs & args) {
		if (!initialised) {
			return;
		}
		auto currentPanel = this->currentPanel.lock();
		MouseArguments action(MouseArguments(args, MouseArguments::Moved, rootGroup->getBounds(), currentPanel, this->mouseOwner));
		if (this->maximised)
			currentPanel->mouseAction(action);
		else {
			rootGroup->mouseAction(action);
			this->updateCurrentPanel();
		}
	}
	
	//----------
	void Controller::mousePressed(ofMouseEventArgs & args) {
		if (!initialised)
			return;
		auto thisMouseClick = pair<long long, ofMouseEventArgs>(ofGetElapsedTimeMillis(), args);

		bool isDoubleClick = (thisMouseClick.first - this->lastMouseClick.first) < OFXCVGUI_DOUBLECLICK_TIME_THRESHOLD_MS;
		isDoubleClick &= thisMouseClick.second.distance(this->lastMouseClick.second) < OFXCVGUI_DOUBLECLICK_SPACE_THRESHOLD_PX;

		if (isDoubleClick) {
			this->mouseOwner = this->lastClickOwner;
		}
		auto currentPanel = this->currentPanel.lock();

		auto action = MouseArguments(args, isDoubleClick ? MouseArguments::Action::DoubleClick : MouseArguments::Action::Pressed, rootGroup->getBounds(), currentPanel, this->mouseOwner);

		if (this->maximised) {
			currentPanel->mouseAction(action);
		}
		else {
			rootGroup->mouseAction(action);
		}

        this->mouseCached = action.global;
		this->mouseOwner = action.getOwner();
		this->lastMouseClick = thisMouseClick;
	}
	
	//----------
	void Controller::mouseReleased(ofMouseEventArgs & args) {
		if (!initialised)
			return;

		auto currentPanel = this->currentPanel.lock();
		MouseArguments action(args, MouseArguments::Released, rootGroup->getBounds(), currentPanel, this->mouseOwner);
		if (this->maximised) {
			currentPanel->mouseAction(action);
		}
		else {
			rootGroup->mouseAction(action);
		}

		this->lastClickOwner = this->mouseOwner;
		this->mouseOwner = nullptr;
	}
	
	//----------
	void Controller::mouseDragged(ofMouseEventArgs & args) {
		if (!initialised)
			return;

		auto currentPanel = this->currentPanel.lock();
		MouseArguments action(args, MouseArguments::Dragged, rootGroup->getBounds(), currentPanel, this->mouseOwner, mouseCached);
		if (this->maximised) {
			currentPanel->mouseAction(action);
		} 
		else {
			rootGroup->mouseAction(action);
		}

		this->mouseCached = action.global;
	}
	
	//----------
	void Controller::keyPressed(ofKeyEventArgs & args) {
		if (args.key == 'f')
			this->toggleFullscreen();
		if (args.key == 'm')
			this->toggleMaximised();

		if (!initialised)
			return;

		auto currentPanel = this->currentPanel.lock();
		KeyboardArguments action(args, KeyboardArguments::Pressed, currentPanel);
		if (this->maximised) {
			//if something is maximised, only it get the key press
			currentPanel->keyboardAction(action);
		}
		else {
			//otherwise everything visible gets the key press
			rootGroup->keyboardAction(action);
		}
	}

	//----------
	void Controller::filesDragged(ofDragInfo & args) {
		if (!initialised)
			return;
		auto rootBounds = this->rootGroup->getBounds();
		auto panel = this->findPanelUnderCursor(rootBounds);
		if (panel != PanelPtr()) {
			auto panelBounds = panel->getBounds();
			ofVec2f panelTopLeft = panelBounds.getTopLeft();
			auto newArgs = FilesDraggedArguments((ofVec2f) args.position - panelTopLeft, (ofVec2f) args.position, args.files);
			panel->onFilesDragged(newArgs);
		}
	}

	//----------
	void Controller::windowResized(ofResizeEventArgs & args) {
		const auto viewportBounds = ofRectangle(0, 0, args.width, args.height);

		auto currentPanel = this->currentPanel.lock();
		if (this->maximised) {
			currentPanel->setBounds(viewportBounds);
		}
		else {
			this->rootGroup->setBounds(viewportBounds);
		}
	}

	//----------
	bool Controller::checkInitialised() {
		if (this->initialised)
			return true;
		else {
			ofLogError("ofxCvGui") << "cannot perform this action as gui is not intialised";
			return false;
		}
	}

	//----------
	PanelPtr Controller::findPanelUnderCursor(ofRectangle & panelBounds, const ofVec2f & position) {
		return PanelPtr(rootGroup->findScreen(position, panelBounds));
	}

	//----------
	void Controller::updateCurrentPanel() {
		if (!this->maximised) {
			auto currentPanelBounds = this->rootGroup->getBounds();
			this->currentPanel = this->findPanelUnderCursor(currentPanelBounds);
			this->currentPanelBounds = currentPanelBounds;
		}
	}
}