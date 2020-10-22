#
# Upload the functions and templates to the appropriate place(s) on S3.
#

VERSION = 10
FUNCTIONS = sfr-lease-$(VERSION).zip odi-lease-$(VERSION).zip check-connectivity-$(VERSION).zip
TEMPLATES = template-$(VERSION).json role-$(VERSION).json bucket-$(VERSION).json security-group-$(VERSION).json
S3TARGETS = $(FUNCTIONS:%=.upload/%) $(TEMPLATES:%=.upload/%)
# Excluding ap-east-1, cn-north-1, cn-northwest-1, and me-south-1 because
# 'The AWS Access Key Id you provided does not exist in our records.'
REGIONS = us-east-1 us-east-2 us-west-1 us-west-2 \
          ca-central-1 \
          eu-west-1 eu-west-2 eu-west-3 eu-central-1 eu-north-1 \
          ap-south-1 ap-northeast-1 ap-northeast-2 ap-southeast-1 ap-southeast-2 \
          sa-east-1

all : s3upload

s3upload : .upload $(S3TARGETS)

.upload :
	mkdir .upload


# Strictly speaking, only the function needs to be in a region-specific
# bucket, but it's much easier to just upload everything everywhere.
$(S3TARGETS) : .upload/% : %
	@aws s3 cp $< s3://condor-annex/$(notdir $<)
	@for region in $(REGIONS); do aws s3 cp $< s3://condor-annex-$${region}/$(notdir $<); done
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
templates : template role bucket security-group
template : template-$(VERSION).json
role : role-$(VERSION).json
bucket : bucket-$(VERSION).json
security-group : security-group-$(VERSION).json

#
# Infastructure management.
#

buckets:
	@for region in $(REGIONS); do echo $${region}; ./make-bucket $${region}; done

amis:
	@for region in $(REGIONS); do echo $${region}; ./copy-default-ami $${region}; done

clean:
	@rm -f $(FUNCTIONS) $(TEMPLATES)
	@rm -fr .upload
