#
# Upload the functions and templates to the appropriate place(s) on S3.
#

VERSION = 5
FUNCTIONS = lease-$(VERSION).zip
TEMPLATES = template-$(VERSION).json
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
	@aws s3 cp $< s3://condor-annex-us-west-1/$(notdir $<)
	@aws s3 cp $< s3://condor-annex-us-west-2/$(notdir $<)
	@mkdir -p $(dir $@)
	@touch $@


#
# Build the function upload from its source.
#
lease-$(VERSION).zip : lease.js
	@rm -f $@
	@zip $@ $^

#
# Build the template upload from its source.
#
template-$(VERSION).json : generate-template
	@./generate-template --version $(VERSION) --pretty-print > $@

#
# Run the template generator (for distribution).
#
template : template-$(VERSION).json
