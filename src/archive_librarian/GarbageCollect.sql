-- 1. Find files to delete (ordered by deletion date, limited by job count target that we calculate)
      -- Creates a temporary table with the FileIds of only the Files that we want to delete
CREATE TEMP TABLE FilesToDelete AS 
SELECT FileId FROM Files 
WHERE DateOfDeletion IS NOT NULL 
ORDER BY DateOfDeletion ASC 
LIMIT ?; -- calculated based on job count needed

-- 2. Collect JobIds 
      -- Finds all JobIds that are in those selected Files 
CREATE TEMP TABLE JobsToDelete AS
SELECT DISTINCT JobId 
FROM JobRecords 
WHERE FileId IN (SELECT FileId FROM FilesToDelete);

-- 3. Collect JobListIds that might become empty
     -- Finds any associated JobListIds to the jobs we're about to delete and saves them to check later
CREATE TEMP TABLE JobListsToCheck AS
SELECT DISTINCT JobListId 
FROM JobRecords 
WHERE JobId IN (SELECT JobId FROM JobsToDelete);

-- 4. Fast deletes using existing indexes
    -- Deletes the marked JobRecords and Jobs from their respective tables
DELETE FROM JobRecords WHERE JobId IN (SELECT JobId FROM JobsToDelete);
DELETE FROM Jobs WHERE JobId IN (SELECT JobId FROM JobsToDelete);

-- 5. Delete empty JobLists
    -- Check the marked JobListIds and see if any of them now have no associated jobs, delete if so
DELETE FROM JobLists 
WHERE JobListId IN (SELECT JobListId FROM JobListsToCheck)
AND JobListId NOT IN (SELECT DISTINCT JobListId FROM Jobs WHERE JobListId IS NOT NULL);

-- 6. Delete files
    -- Delete the File entries that we had previously marked
DELETE FROM Files WHERE FileId IN (SELECT FileId FROM FilesToDelete);