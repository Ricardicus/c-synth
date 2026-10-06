#pragma once
#include "PluginProcessor.h"

// Draft recordings live in the processor so closing the editor doesn't lose edits.
class SamplePanel final : public juce::Component, private juce::ListBoxModel {
public:
    explicit SamplePanel(CSynthProcessor& p) : processor(p), list("Recordings",this)
    {
        for (auto* c : std::initializer_list<juce::Component*>{&add,&remove,&apply,&fm,&samples,&list,&heading,&frequencyHeading,&info,&help}) addAndMakeVisible(c);
        list.setRowHeight(42); list.setColour(juce::ListBox::backgroundColourId,juce::Colour(0xff182333));
        heading.setText("Recordings to apply",juce::dontSendNotification);
        frequencyHeading.setText("Base frequency (Hz)",juce::dontSendNotification);
        help.setText("A3 = 220.00 Hz, A4 = 440.00 Hz, A5 = 880.00 Hz. One recording works too.\n"
            "ADSR, filters, echo and reverb stay active. Files play once; ADSR cannot extend their length.",juce::dontSendNotification);
        help.setFont(juce::FontOptions(13.0f));
        add.onClick=[this] { browse(); };
        remove.onClick=[this] {
            auto files=processor.sampleFiles(); int index=list.getSelectedRow();
            if (index>=0 && index<static_cast<int>(files.size())) {
                files.erase(files.begin()+index); processor.setSampleFiles(std::move(files)); list.deselectAllRows();
                message="Draft changed. Apply files to replace the active bank."; refresh();
            }
        };
        apply.onClick=[this] {
            auto result=processor.applySampleFiles();
            message=result.wasOk() ? "Applied. Play MIDI from Logic or use the Keyboard tab." : result.getErrorMessage(); refresh();
        };
        fm.onClick=[this] { report(processor.useSamples(false)); };
        samples.onClick=[this] { report(processor.useSamples(true)); };
        refresh();
    }
    void refresh()
    {
        auto files=processor.sampleFiles(); bool changed=files.size()!=rows.size();
        if (!changed) for (std::size_t i=0;i<files.size();++i)
            if (files[i].path!=rows[i].path || files[i].hz!=rows[i].hz) { changed=true; break; }
        rows=std::move(files);
        if (changed) list.updateContent();
        fm.setToggleState(!processor.usesSamples(),juce::dontSendNotification);
        samples.setToggleState(processor.usesSamples(),juce::dontSendNotification);
        samples.setEnabled(processor.sampleBankSize()>0);
        samples.setTooltip(processor.sampleBankSize()>0 ? "Switch to the applied sample bank." : "Add recordings and click Apply files first.");
        remove.setEnabled(list.getSelectedRow()>=0); apply.setEnabled(!rows.empty());
        auto error=processor.sampleLoadError();
        info.setText("Source: "+juce::String(processor.usesSamples() ? "Samples" : "FM")+" | Bank: "+juce::String(processor.sampleBankSize())
            +" | Draft: "+juce::String(static_cast<int>(rows.size()))+"\n"+(error.isNotEmpty() ? error : message),juce::dontSendNotification);
    }
    void resized() override
    {
        auto area=getLocalBounds(); auto buttons=area.removeFromTop(34);
        for (auto* b : {&add,&remove,&apply,&fm,&samples}) b->setBounds(buttons.removeFromLeft(getWidth()/5).reduced(0,2).withTrimmedRight(8));
        area.removeFromTop(6); auto headers=area.removeFromTop(24);
        frequencyHeading.setBounds(headers.removeFromRight(198)); heading.setBounds(headers);
        help.setBounds(area.removeFromBottom(42)); info.setBounds(area.removeFromBottom(46));
        area.removeFromBottom(5); list.setBounds(area);
    }
private:
    struct Row final : juce::Component {
        Row(SamplePanel& p) : owner(p)
        {
            addAndMakeVisible(name); addAndMakeVisible(hz);
            hz.setInputRestrictions(23,"0123456789.eE+-"); hz.setSelectAllWhenFocused(true);
            hz.onTextChange=[this] {
                auto files=owner.processor.sampleFiles();
                if (index<0 || index>=static_cast<int>(files.size())) return;
                files[static_cast<std::size_t>(index)].hz=hz.getText(); owner.processor.setSampleFiles(std::move(files));
                owner.message="Draft changed. Apply files to update the instrument.";
            };
            hz.onReturnKey=[this] { hz.giveAwayKeyboardFocus(); };
        }
        void resized() override {
            auto area=getLocalBounds().reduced(8,5); hz.setBounds(area.removeFromRight(190));
            name.setBounds(area.withTrimmedRight(12));
        }
        void mouseDown(const juce::MouseEvent&) override { owner.list.selectRow(index); }
        SamplePanel& owner; int index=-1; juce::Label name; juce::TextEditor hz;
    };
    int getNumRows() override { return static_cast<int>(rows.size()); }
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override {}
    juce::Component* refreshComponentForRow(int index,bool selected,juce::Component* existing) override
    {
        std::unique_ptr<Row> row(dynamic_cast<Row*>(existing));
        if (index<0 || index>=static_cast<int>(rows.size())) return nullptr;
        if (!row) row=std::make_unique<Row>(*this);
        row->index=index; const auto& file=rows[static_cast<std::size_t>(index)];
        row->name.setText(juce::String(index+1)+". "+juce::File(file.path).getFileName(),juce::dontSendNotification);
        row->name.setTooltip(file.path); row->name.setInterceptsMouseClicks(false,false);
        row->name.setColour(juce::Label::backgroundColourId,selected ? juce::Colour(0xff21666d) : juce::Colours::transparentBlack);
        row->hz.setTooltip("The pitch recorded in this file, in Hz. Click to replace; Apply files commits edits.");
        if (!row->hz.hasKeyboardFocus(true)) row->hz.setText(file.hz,false);
        return row.release();
    }
    void selectedRowsChanged(int) override { remove.setEnabled(list.getSelectedRow()>=0); }
    void report(juce::Result result) { message=result.wasOk() ? "Source changed." : result.getErrorMessage(); refresh(); }
    void browse()
    {
        chooser=std::make_unique<juce::FileChooser>("Add sample recordings",juce::File{},"*.wav;*.mp3");
        juce::Component::SafePointer<SamplePanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
            [safe](const juce::FileChooser& selected) {
                if (!safe) return;
                auto files=safe->processor.sampleFiles();
                for (const auto& file : selected.getResults()) {
                    if (files.size()==SYNTH_SAMPLE_MAX_ENTRIES) { safe->message="Maximum 128 recordings per bank."; break; }
                    files.push_back({file.getFullPathName(),"440.00"});
                    safe->message="Set each recording's base frequency, then click Apply files.";
                }
                safe->processor.setSampleFiles(std::move(files)); safe->refresh();
            });
    }
    CSynthProcessor& processor;
    std::vector<CSynthProcessor::SampleFile> rows;
    juce::TextButton add{"Add sound files"}, remove{"Remove selected"}, apply{"Apply files"}, fm{"FM source"}, samples{"Sample source"};
    juce::ListBox list; juce::Label heading,frequencyHeading,info,help;
    juce::String message{"Add WAV or MP3 files, enter their pitch in Hz, and apply the bank."};
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplePanel)
};
