#
# Upload the functions and templates to the appropriate place(s) on S3.
#

VERSION = 7
FUNCTIONS = sfr-lease-$(VERSION).zip odi-lease-$(VERSION).zip check-connectivity-$(VERSION).zip
TEMPLATES = template-$(VERSION).json role-$(VERSION).json bucket-$(VERSION).json security-group-$(VERSION).json
S3TARGETS = $(FUNCTIONS:%=.upload/%) $(TEMPLATES:%=.upload/%)

all : s3upload

s3upload : .upload $(S3TARGETS)

.upload :
	mkdir .upload

# Strictly speaking, only the function needs to be in a region-specific
# bucket, but it's much easier to just upload everything everywhere.
$(S3TARGETS) : .upload/% : %
	@aws s3 cp $< s3://condor-annex/$(notdir $<)
	@aws s3 cp $< s3://condor-annex-us-east-1/$(notdir $<)
	@aws s3 cp $< s3://condor-annex-us-east-2/$(notdir $<)
	@aws s3 cp $< s3://condor-annex-us-west-1/$(notdir $<)
	@aws s3 cp $< s3://condor-annex-us-west-2/$(notdir $<)
	@mkdir -p $(dir $@)
	@touch $@


#
# Build the function upload from its source.
#
sfr-lease-$(VERSION).zip : sfr-lease.js
	@rm -f $@
	@zip $@ $^

odi-lease-$(VERSION).zip : odi-lease.js
	@rm -f $@
	@zip $@ $^

check-connectivity-$(VERSION).zip : check-connectivity.py
	@rm -f $@
	@zip $@ $^

#
# Build the template upload from its source.
#
template-$(VERSION).json : generate-template
	@./generate-template --version $(VERSION) --pretty-print > $@

role-$(VERSION).json : generate-role
	@./generate-role --pretty-print > $@

bucket-$(VERSION).json : generate-bucket
	@./generate-bucket --pretty-print > $@

security-group-$(VERSION).json : generate-security-group
	@./generate-security-group --pretty-print > $@

#
# Run the template generator (for distribution).
#
templates : template role bucket
template : template-$(VERSION).json
role : role-$(VERSION).json
bucket : bucket-$(VERSION).json
security-group : security-group-$(VERSION).json
