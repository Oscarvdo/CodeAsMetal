/* CodeAsMetal V1: execute in a NEW database named CodeAsMetal using SSMS.
   SQL Server 2019+; compatibility >= 150. No DROP/DELETE of existing data.
   Application logic lives in C++; procedures enforce storage invariants.
   The DBA grants cam_app to authorized Windows users/groups separately. */
SET XACT_ABORT ON;
GO
IF SCHEMA_ID(N'cam') IS NULL EXEC(N'CREATE SCHEMA cam AUTHORIZATION dbo');
GO
IF OBJECT_ID(N'cam.Projects') IS NULL
CREATE TABLE cam.Projects (
 Id uniqueidentifier NOT NULL CONSTRAINT PK_Projects PRIMARY KEY,
 Name nvarchar(200) NOT NULL,
 Document nvarchar(max) NOT NULL CONSTRAINT CK_Project_JSON CHECK(ISJSON(Document)=1),
 ModifiedUtc datetime2(3) NOT NULL CONSTRAINT DF_Project_UTC DEFAULT SYSUTCDATETIME(),
 ModifiedBy sysname NOT NULL CONSTRAINT DF_Project_Actor DEFAULT ORIGINAL_LOGIN(),
 Version rowversion NOT NULL
);
GO
IF OBJECT_ID(N'cam.ProjectHistory') IS NULL
CREATE TABLE cam.ProjectHistory (
 AuditId bigint IDENTITY PRIMARY KEY,
 ProjectId uniqueidentifier NOT NULL REFERENCES cam.Projects(Id),
 Document nvarchar(max) NOT NULL,
 SavedUtc datetime2(3) NOT NULL DEFAULT SYSUTCDATETIME(),
 Actor sysname NOT NULL DEFAULT ORIGINAL_LOGIN()
);
GO
IF OBJECT_ID(N'cam.Estimates') IS NULL
CREATE TABLE cam.Estimates (
 Id uniqueidentifier NOT NULL PRIMARY KEY,
 ProjectId uniqueidentifier NOT NULL REFERENCES cam.Projects(Id),
 RevisionId uniqueidentifier NOT NULL,
 Snapshot nvarchar(max) NOT NULL CHECK(ISJSON(Snapshot)=1),
 IssuedUtc datetime2(3) NOT NULL DEFAULT SYSUTCDATETIME(),
 IssuedBy sysname NOT NULL DEFAULT ORIGINAL_LOGIN()
);
GO
CREATE OR ALTER PROCEDURE cam.ProjectList AS
BEGIN SET NOCOUNT ON; SELECT Id,Name FROM cam.Projects ORDER BY Name,Id; END;
GO
CREATE OR ALTER PROCEDURE cam.ProjectLoad @Id uniqueidentifier AS
BEGIN SET NOCOUNT ON; SELECT Document,Version FROM cam.Projects WHERE Id=@Id; END;
GO
CREATE OR ALTER PROCEDURE cam.ProjectSave
 @Id uniqueidentifier,@Name nvarchar(200),@Document nvarchar(max),@ExpectedVersion binary(8)=NULL
AS
BEGIN
 SET NOCOUNT ON; SET XACT_ABORT ON;
 IF ISJSON(@Document)<>1 THROW 51000,'Document must be JSON.',1;
 IF ISNULL(TRY_CONVERT(int,JSON_VALUE(@Document,'$.schema')),0)<>1
    OR ISNULL(TRY_CONVERT(uniqueidentifier,JSON_VALUE(@Document,'$.id')),'00000000-0000-0000-0000-000000000000')<>@Id
    THROW 51001,'Document identity/schema mismatch.',1;
 IF NULLIF(LTRIM(RTRIM(@Name)),N'') IS NULL THROW 51002,'Name required.',1;
 DECLARE @Incoming TABLE(Id uniqueidentifier PRIMARY KEY,RevisionId uniqueidentifier,Snapshot nvarchar(max));
 INSERT @Incoming(Id,RevisionId,Snapshot)
 SELECT CONVERT(uniqueidentifier,JSON_VALUE(e.value,'$.id')),
        CONVERT(uniqueidentifier,JSON_VALUE(r.value,'$.id')),e.value
 FROM OPENJSON(@Document,'$.revisions') r CROSS APPLY OPENJSON(r.value,'$.estimates') e;
 BEGIN TRY
  BEGIN TRANSACTION;
  DECLARE @Actual binary(8);
  SELECT @Actual=Version FROM cam.Projects WITH(UPDLOCK,HOLDLOCK) WHERE Id=@Id;
  IF @Actual IS NULL
  BEGIN
    IF @ExpectedVersion IS NOT NULL THROW 51003,'Project removed or wrong server. Reload.',1;
    INSERT cam.Projects(Id,Name,Document) VALUES(@Id,@Name,@Document);
  END
  ELSE
  BEGIN
    IF @ExpectedVersion IS NULL OR @ExpectedVersion<>@Actual
       THROW 51004,'Concurrency conflict. Reload; your local recovery copy is retained.',1;
    /* Issued estimates cannot be edited or removed, even through a forged client. */
    IF EXISTS(SELECT 1 FROM cam.Estimates old LEFT JOIN @Incoming i ON i.Id=old.Id
              WHERE old.ProjectId=@Id AND (i.Id IS NULL OR old.RevisionId<>i.RevisionId OR
                HASHBYTES('SHA2_256',old.Snapshot)<>HASHBYTES('SHA2_256',i.Snapshot)))
       THROW 51005,'Issued estimate is immutable.',1;
    UPDATE cam.Projects SET Name=@Name,Document=@Document,ModifiedUtc=SYSUTCDATETIME(),ModifiedBy=ORIGINAL_LOGIN()
    WHERE Id=@Id;
  END;
  IF EXISTS(SELECT 1 FROM @Incoming i JOIN cam.Estimates e ON e.Id=i.Id WHERE e.ProjectId<>@Id)
     THROW 51006,'Estimate identity already belongs to another project.',1;
  INSERT cam.Estimates(Id,ProjectId,RevisionId,Snapshot)
   SELECT i.Id,@Id,i.RevisionId,i.Snapshot FROM @Incoming i WHERE NOT EXISTS(SELECT 1 FROM cam.Estimates e WHERE e.Id=i.Id);
  INSERT cam.ProjectHistory(ProjectId,Document) VALUES(@Id,@Document);
  DECLARE @SavedVersion binary(8);
  SELECT @SavedVersion=Version FROM cam.Projects WHERE Id=@Id;
  COMMIT;
  SELECT @SavedVersion AS Version;
 END TRY
 BEGIN CATCH
  IF @@TRANCOUNT>0 ROLLBACK;
  THROW;
 END CATCH
END;
GO
IF DATABASE_PRINCIPAL_ID(N'cam_app') IS NULL CREATE ROLE cam_app;
GO
GRANT EXECUTE ON SCHEMA::cam TO cam_app;
DENY INSERT,UPDATE,DELETE ON cam.Projects TO cam_app;
DENY INSERT,UPDATE,DELETE ON cam.ProjectHistory TO cam_app;
DENY INSERT,UPDATE,DELETE ON cam.Estimates TO cam_app;
GO
/* Example DBA action, replace with actual authorized domain account:
 CREATE USER [DOMAIN\Engineering] FOR LOGIN [DOMAIN\Engineering];
 ALTER ROLE cam_app ADD MEMBER [DOMAIN\Engineering];
 Do not grant db_owner to application users. All cam_app members share project access.
 Row-level tenancy/authorization is outside this single-organization release. */
